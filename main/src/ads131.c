#include "ads131.h"
#include "esp_log.h"
#include <string.h>

#define ADS131_REG_ID        0x00
#define ADS131_REG_CLOCK     0x03
#define ADS131_REG_CH0_CFG   0x05
#define ADS131_REG_CH1_CFG   0x06
#define ADS131_REG_CH2_CFG   0x07
#define ADS131_REG_CH3_CFG   0x08

#define ADS131_WORD_BYTES   3
#define ADS131_FRAME_WORDS  6
#define ADS131_FRAME_BYTES  (ADS131_WORD_BYTES * ADS131_FRAME_WORDS)

#define ADS131_REG_CLOCK 0x03

static const char *TAG = "ADS131";

static esp_err_t spi_xfer(ads131_t *dev, const uint8_t *tx, uint8_t *rx, size_t len);
static esp_err_t ads131_write_reg(ads131_t *dev, uint8_t reg, uint16_t value);
static esp_err_t ads131_read_reg(ads131_t *dev, uint8_t reg, uint16_t *out);



static esp_err_t ads131_xfer_frame(ads131_t *dev, const uint8_t tx[ADS131_FRAME_BYTES],
                                  uint8_t rx[ADS131_FRAME_BYTES])
{
    spi_transaction_t t = {
        .length = ADS131_FRAME_BYTES * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    gpio_set_level(dev->cs_gpio, 0);
    esp_err_t ret = spi_device_transmit(dev->spi, &t);
    gpio_set_level(dev->cs_gpio, 1);
    return ret;
}


static inline void put_cmd24(uint8_t *w3, uint16_t cmd16)
{
    w3[0] = (uint8_t)(cmd16 >> 8);
    w3[1] = (uint8_t)(cmd16 & 0xFF);
    w3[2] = 0x00; // padding چون wordlength=24
}

static inline uint16_t get_resp16_from_word24(const uint8_t *w3)
{
    return (uint16_t)((w3[0] << 8) | w3[1]); // دو بایت بالایی
}


static esp_err_t ads131_write_reg(ads131_t *dev, uint8_t addr, uint16_t value)
{
    // WREG command word: 011 aaaaa a nnnnnnn  (n=0 for 1 reg)
    uint16_t cmd = (uint16_t)(0x6000 | ((addr & 0x1F) << 7) | 0x0000);

    uint8_t tx[ADS131_FRAME_BYTES] = {0};
    uint8_t rx[ADS131_FRAME_BYTES] = {0};

    // frame #1:
    // word0 = WREG cmd
    // word1 = register data (16-bit MSB-aligned in 24-bit word)
    put_cmd24(&tx[0], cmd);
    put_cmd24(&tx[3], value);
    ESP_ERROR_CHECK(ads131_xfer_frame(dev, tx, rx));

    // frame #2: NULL (برای اینکه command pipeline تمیز شود)
    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    put_cmd24(&tx[0], 0x0000);
    ESP_ERROR_CHECK(ads131_xfer_frame(dev, tx, rx));

    return ESP_OK;
}


static esp_err_t ads131_read_reg(ads131_t *dev, uint8_t addr, uint16_t *out)
{
    // RREG command word: 101 aaaaa a nnnnnnn  (n=0 for 1 reg) 
    uint16_t cmd = (uint16_t)(0xA000 | ((addr & 0x1F) << 7) | 0x0000);

    uint8_t tx[ADS131_FRAME_BYTES] = {0};
    uint8_t rx[ADS131_FRAME_BYTES] = {0};

    // frame #1: send command
    put_cmd24(&tx[0], cmd);
    ESP_ERROR_CHECK(ads131_xfer_frame(dev, tx, rx));

    // frame #2: send NULL (0x0000) to receive response in word0
    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    put_cmd24(&tx[0], 0x0000); // NULL
    ESP_ERROR_CHECK(ads131_xfer_frame(dev, tx, rx));

    *out = get_resp16_from_word24(&rx[0]); // response word of frame #2
    return ESP_OK;
}


esp_err_t ads131_set_gain_1_all(ads131_t *dev)
{
    uint16_t cfg = 0x0000; // PGA_GAIN = 000 => Gain = 1

    ESP_ERROR_CHECK(ads131_command(dev, 0x0011)); // SDATAC
    vTaskDelay(pdMS_TO_TICKS(2));

    ESP_ERROR_CHECK(ads131_write_reg(dev, ADS131_REG_CH0_CFG, cfg));
    ESP_ERROR_CHECK(ads131_write_reg(dev, ADS131_REG_CH1_CFG, cfg));
    ESP_ERROR_CHECK(ads131_write_reg(dev, ADS131_REG_CH2_CFG, cfg));
    ESP_ERROR_CHECK(ads131_write_reg(dev, ADS131_REG_CH3_CFG, cfg));

    ESP_ERROR_CHECK(ads131_command(dev, 0x0010)); // RDATAC
    vTaskDelay(pdMS_TO_TICKS(2));

    return ESP_OK;
}

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

    // CS
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << cs_gpio),
        .mode = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(cs_gpio, 1);

    // DRDY input + ISR
    gpio_config_t drdy = {
        .pin_bit_mask = (1ULL << drdy_gpio),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&drdy));

    // install ISR service once
    static bool isr_installed = false;
    if (!isr_installed) {
        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        isr_installed = true;
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(drdy_gpio, drdy_isr, dev));

    // RESET
    gpio_config_t rst = {
        .pin_bit_mask = (1ULL << reset_gpio),
        .mode = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&rst));

    // SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = miso_io,
        .mosi_io_num = mosi_io,
        .sclk_io_num = sclk_io,
        .max_transfer_sz = 32  // >= 15 bytes
    };
    ESP_ERROR_CHECK(spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = spi_freq_hz,
        .mode = 1,                 // CPOL=0, CPHA=1  (خیلی مهم)
        .spics_io_num = -1,
        .queue_size = 1
    };
    ESP_ERROR_CHECK(spi_bus_add_device(host, &devcfg, &dev->spi));

    // HW reset
    gpio_set_level(reset_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(reset_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // stop/start continuous read (تمیز)
    ESP_ERROR_CHECK(ads131_command(dev, 0x0011)); // SDATAC
    vTaskDelay(pdMS_TO_TICKS(2));

    // (اختیاری) خواندن ID برای اطمینان
    uint16_t id = 0;
    if (ads131_read_reg(dev, ADS131_REG_ID, &id) == ESP_OK) {
        ESP_LOGI(TAG, "ID=0x%04X", (unsigned)id);
    }

    ESP_ERROR_CHECK(ads131_command(dev, 0x0010)); // RDATAC
    vTaskDelay(pdMS_TO_TICKS(2));

    ESP_ERROR_CHECK(ads131_set_gain_1_all(dev));

    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

esp_err_t ads131_command(ads131_t *dev, uint16_t cmd)
{
    uint8_t tx[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
    return spi_xfer(dev, tx, NULL, sizeof(tx));
}

bool ads131_wait_drdy(ads131_t *dev, TickType_t timeout)
{
    return xSemaphoreTake(dev->drdy_sem, timeout) == pdTRUE;
}

esp_err_t ads131_read_frame(ads131_t *dev, ads131_frame_t *frame)
{
    uint8_t rx[15] = {0};
    uint8_t tx[15] = {0};

    esp_err_t ret = spi_xfer(dev, tx, rx, sizeof(rx));
    if (ret != ESP_OK) return ret;

    uint32_t s = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | rx[2];
    frame->status = (s & 0x800000) ? (int32_t)(s | 0xFF000000) : (int32_t)s;

    for (int i = 0; i < 4; i++) {
        int idx = 3 + i * 3;
        uint32_t v = ((uint32_t)rx[idx] << 16) | ((uint32_t)rx[idx + 1] << 8) | rx[idx + 2];
        frame->ch[i] = (v & 0x800000) ? (int32_t)(v | 0xFF000000) : (int32_t)v;
    }

    return ESP_OK;
}

/* ================== Data rate درست: با تنظیم رجیستر CLOCK ==================
   فرض: CLKIN را خودت با LEDC روی GPIO17 می‌دهی (مثلاً 4.096MHz).
   اینجا فقط OSR و HR mode را تنظیم می‌کنیم.
*/
esp_err_t ads131_set_data_rate(ads131_t *dev, ads131_data_rate_t rate)
{
    uint16_t clock = 0;
    ESP_ERROR_CHECK(ads131_read_reg(dev, ADS131_REG_CLOCK, &clock));

    // پاک کردن OSR[2:0] (بیت‌های 4..2)
    clock &= ~(0x7u << 2);

    uint16_t osr_code = 0b011; // default = 1024

    // OSR codes (از دیتاشیت):
    // 001=256, 010=512, 011=1024, 100=2048 ...
    switch (rate) {
        case ADS131_RATE_4KSPS:  osr_code = 0b001; break; // OSR=256 => 4kSPS @ 2.048MHz (VLP)
        case ADS131_RATE_2KSPS:  osr_code = 0b010; break; // OSR=512
        case ADS131_RATE_1KSPS:  osr_code = 0b011; break; // OSR=1024
        case ADS131_RATE_500SPS: osr_code = 0b100; break; // OSR=2048
        default: osr_code = 0b001; break;
    }

    clock |= (osr_code << 2);

    ESP_ERROR_CHECK(ads131_write_reg(dev, ADS131_REG_CLOCK, clock));

    uint16_t rb = 0;
    ESP_ERROR_CHECK(ads131_read_reg(dev, ADS131_REG_CLOCK, &rb));
    ESP_LOGI(TAG, "CLOCK after set: 0x%04X (readback 0x%04X)", (unsigned)clock, (unsigned)rb);

    return ESP_OK;
}



void ads131_deinit(ads131_t *dev)
{
    gpio_isr_handler_remove(dev->drdy_gpio);
    vSemaphoreDelete(dev->drdy_sem);
}
