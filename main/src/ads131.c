#include "ads131.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "string.h"

static const char *TAG = "ads131";

// DRDY ISR: فقط semaphore را give می‌کنیم
static void IRAM_ATTR drdy_isr(void* arg) {
    ads131_t *dev = (ads131_t*)arg;
    BaseType_t hpw = pdFALSE;
    xSemaphoreGiveFromISR(dev->drdy_sem, &hpw);
    if (hpw) portYIELD_FROM_ISR();
}

esp_err_t ads131_init(ads131_t *dev, spi_host_device_t host,
                      int miso_io, int mosi_io, int sclk_io,
                      gpio_num_t cs_gpio, gpio_num_t drdy_gpio, gpio_num_t reset_gpio,
                      int spi_freq_hz)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    memset(dev, 0, sizeof(*dev));
    dev->cs_gpio = cs_gpio;
    dev->drdy_gpio = drdy_gpio;
    dev->reset_gpio = reset_gpio;
    dev->drdy_sem = xSemaphoreCreateBinary();
    if (!dev->drdy_sem) {
        ESP_LOGE(TAG, "cannot create semaphore");
        return ESP_ERR_NO_MEM;
    }

    // init CS pin as manual gpio (we'll use spi_device but keep CS as gpio manually)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL<<cs_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(cs_gpio, 1);

    // configure DRDY input with ISR
    gpio_config_t drdy_conf = {
        .pin_bit_mask = (1ULL<<drdy_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE // DRDY usually falls when data ready
    };
    gpio_config(&drdy_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(drdy_gpio, drdy_isr, dev);

    // reset pin
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL<<reset_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rst_conf);
    gpio_set_level(reset_gpio, 1);

    // SPI bus init (master)
    spi_bus_config_t buscfg = {
        .miso_io_num = miso_io,
        .mosi_io_num = mosi_io,
        .sclk_io_num = sclk_io,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64 // adjust for frame size
    };
    esp_err_t ret = spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %d", ret);
        return ret;
    }

    // add device (we'll use manual CS via gpio to have full control)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = spi_freq_hz,
        .mode = 0,
        .spics_io_num = -1, // no auto CS
        .queue_size = 3,
    };
    ret = spi_bus_add_device(host, &devcfg, &dev->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %d", ret);
        return ret;
    }

    // small reset pulse
    gpio_set_level(reset_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(reset_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "ADS131 initialized (SCLK:%d MOSI:%d MISO:%d CS:%d DRDY:%d)",
             sclk_io, mosi_io, miso_io, cs_gpio, drdy_gpio);
    return ESP_OK;
}

void ads131_deinit(ads131_t *dev)
{
    if (!dev) return;
    if (dev->spi) {
        spi_bus_remove_device(dev->spi);
        dev->spi = NULL;
    }
    // do not uninit bus here (leave it)
    if (dev->drdy_sem) {
        vSemaphoreDelete(dev->drdy_sem);
        dev->drdy_sem = NULL;
    }
    gpio_isr_handler_remove(dev->drdy_gpio);
}

// helper low-level SPI transfer
static esp_err_t ads131_spi_transfer(ads131_t *dev, const uint8_t *tx, uint8_t *rx, int len)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    // manual CS
    gpio_set_level(dev->cs_gpio, 0);
    esp_err_t ret = spi_device_transmit(dev->spi, &t);
    gpio_set_level(dev->cs_gpio, 1);
    return ret;
}

esp_err_t ads131_command(ads131_t *dev, uint16_t cmd)
{
    uint8_t tx[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return ads131_spi_transfer(dev, tx, NULL, 2);
}

esp_err_t ads131_read_register(ads131_t *dev, uint8_t address, uint16_t *value)
{
    // Device-specific: for ADS131M04/08 registers are 16-bit, use RREG command format as per datasheet
    uint8_t tx[4];
    uint8_t rx[4];
    // For many ADS131 devices: command = 0x20 | (num-1) ??? (check datasheet). Here we implement a generic: send RREG addr, then read 2 bytes
    // Example basic approach: send command 0x20 addr  then read two bytes
    tx[0] = 0x20; // RREG command (example — verify for your part!)
    tx[1] = address;
    tx[2] = 0x00;
    tx[3] = 0x00;
    esp_err_t ret = ads131_spi_transfer(dev, tx, rx, 4);
    if (ret == ESP_OK && value) {
        *value = ((uint16_t)rx[2] << 8) | rx[3];
    }
    return ret;
}

esp_err_t ads131_write_register(ads131_t *dev, uint8_t address, uint16_t value)
{
    uint8_t tx[4];
    tx[0] = 0x40; // WREG command (example — verify for your part!)
    tx[1] = address;
    tx[2] = (value >> 8) & 0xFF;
    tx[3] = value & 0xFF;
    return ads131_spi_transfer(dev, tx, NULL, 4);
}

bool ads131_wait_drdy(ads131_t *dev, TickType_t ticks_to_wait)
{
    if (!dev || !dev->drdy_sem) return false;
    return xSemaphoreTake(dev->drdy_sem, ticks_to_wait) == pdTRUE;
}

esp_err_t ads131_read_frame_raw(ads131_t *dev, int32_t *out_samples, size_t channels)
{
    // frame size: for each channel 3 bytes (24-bit). total bytes = 3*channels + maybe status bytes.
    size_t bytes = channels * 3;
    // allocate buffers on stack if small, or use calloc for larger channels
    uint8_t txbuf[64];
    uint8_t rxbuf[64];
    memset(txbuf, 0, sizeof(txbuf));
    memset(rxbuf, 0, sizeof(rxbuf));
    // perform transfer
    esp_err_t ret = ads131_spi_transfer(dev, txbuf, rxbuf, bytes);
    if (ret != ESP_OK) return ret;
    // convert 24-bit two's complement to int32
    for (size_t i = 0; i < channels; ++i) {
        uint32_t v = ((uint32_t)rxbuf[i*3] << 16) | ((uint32_t)rxbuf[i*3+1] << 8) | rxbuf[i*3+2];
        // sign extend 24->32
        if (v & 0x800000) v |= 0xFF000000;
        out_samples[i] = (int32_t)v;
    }
    return ESP_OK;
}
