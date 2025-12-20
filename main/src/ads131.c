#include "ads131.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ADS131";

static void IRAM_ATTR drdy_isr(void *arg)
{
    ads131_t *dev = (ads131_t *)arg;
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(dev->drdy_sem, &hp);
    if (hp) portYIELD_FROM_ISR();
}

static esp_err_t spi_xfer(ads131_t *dev, const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    gpio_set_level(dev->cs_gpio, 0);
    esp_err_t ret = spi_device_transmit(dev->spi, &t);
    gpio_set_level(dev->cs_gpio, 1);

    return ret;
}

esp_err_t ads131_init(
    ads131_t *dev,
    spi_host_device_t host,
    int miso_io,
    int mosi_io,
    int sclk_io,
    gpio_num_t cs_gpio,
    gpio_num_t drdy_gpio,
    gpio_num_t reset_gpio,
    int spi_freq_hz)
{
    memset(dev, 0, sizeof(*dev));

    dev->cs_gpio = cs_gpio;
    dev->drdy_gpio = drdy_gpio;
    dev->reset_gpio = reset_gpio;
    dev->drdy_sem = xSemaphoreCreateBinary();

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cs_gpio),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io);
    gpio_set_level(cs_gpio, 1);

    gpio_config_t drdy = {
        .pin_bit_mask = (1ULL << drdy_gpio),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&drdy);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(drdy_gpio, drdy_isr, dev);

    gpio_config_t rst = {
        .pin_bit_mask = (1ULL << reset_gpio),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&rst);

    spi_bus_config_t buscfg = {
        .miso_io_num = miso_io,
        .mosi_io_num = mosi_io,
        .sclk_io_num = sclk_io,
        .max_transfer_sz = 32
    };
    spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = spi_freq_hz,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1
    };
    spi_bus_add_device(host, &devcfg, &dev->spi);

    gpio_set_level(reset_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(reset_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    ads131_command(dev, 0x11); // SDATAC
    vTaskDelay(pdMS_TO_TICKS(2));
    ads131_command(dev, 0x10); // RDATAC

    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

esp_err_t ads131_command(ads131_t *dev, uint16_t cmd)
{
    uint8_t tx[2] = { cmd >> 8, cmd & 0xFF };
    return spi_xfer(dev, tx, NULL, 2);
}

bool ads131_wait_drdy(ads131_t *dev, TickType_t timeout)
{
    return xSemaphoreTake(dev->drdy_sem, timeout) == pdTRUE;
}

esp_err_t ads131_read_frame(ads131_t *dev, ads131_frame_t *frame)
{
    uint8_t rx[15] = {0};
    uint8_t tx[15] = {0};

    esp_err_t ret = spi_xfer(dev, tx, rx, 15);
    if (ret != ESP_OK) return ret;

    uint32_t s =
        (rx[0] << 16) |
        (rx[1] << 8)  |
        rx[2];
    frame->status = (s & 0x800000) ? (s | 0xFF000000) : s;

    if (frame->status == 0x7FFFFF) return ESP_ERR_INVALID_STATE;

    for (int i = 0; i < 4; i++) {
        int idx = 3 + i * 3;
        uint32_t v =
            (rx[idx] << 16) |
            (rx[idx+1] << 8) |
            rx[idx+2];
        frame->ch[i] = (v & 0x800000) ? (v | 0xFF000000) : v;
    }

    return ESP_OK;
}

esp_err_t ads131_set_data_rate(ads131_t *dev, ads131_data_rate_t rate)
{
    uint16_t clock = 0x8000;
    if (rate == ADS131_RATE_3906SPS) clock = 0x4000;
    if (rate == ADS131_RATE_7812SPS) clock = 0x3000;

    ads131_command(dev, 0x11);
    vTaskDelay(pdMS_TO_TICKS(2));

    uint8_t tx[4] = { 0x40, 0x00, clock >> 8, clock & 0xFF };
    spi_xfer(dev, tx, NULL, 4);

    ads131_command(dev, 0x10);
    vTaskDelay(pdMS_TO_TICKS(2));

    return ESP_OK;
}

void ads131_deinit(ads131_t *dev)
{
    gpio_isr_handler_remove(dev->drdy_gpio);
    vSemaphoreDelete(dev->drdy_sem);
}
