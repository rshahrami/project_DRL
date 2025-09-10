#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2cdev.h"            // I2C NG (بدون driver/i2c.h)
#include "ads1115_oneshot.h"   // پیاده‌سازی oneshot خودت

static const char *TAG = "main";

// پین‌های I2C (در صورت نیاز تغییر بده)
#define I2C_PORT     I2C_NUM_0
#define I2C_SDA_GPIO 18
#define I2C_SCL_GPIO 17

// کانال‌ها
#define CH0 0
#define CH1 1

// نرخ بیشینهٔ ADS1115
#define DR_MAX ADS111X_DATA_RATE_860   // 860 SPS

static ads1115_ctx_t g_adc;

static void ads_reader_task(void *arg)
{
    (void)arg;

    int16_t raw0 = 0, raw1 = 0;
    float   v0   = 0.0f, v1 = 0.0f;

    while (1) {
        // CH0
        if (ads1115_read_single_ended(&g_adc, CH0, &raw0, &v0) != ESP_OK) {
            ESP_LOGW(TAG, "ADS read CH0 failed");
        }
        // CH1
        if (ads1115_read_single_ended(&g_adc, CH1, &raw1, &v1) != ESP_OK) {
            ESP_LOGW(TAG, "ADS read CH1 failed");
        }

        // توجه: با 860SPS و خواندن دو کانال به‌صورت نوبتی،
        // نرخ موثر هر کانال ≈ 430 نمونه‌برثانیه خواهد بود.
        ESP_LOGI("ADS", "CH0 raw=%6d  V=%7.4f | CH1 raw=%6d  V=%7.4f", raw0, v0, raw1, v1);

        // یک استراحت خیلی کوتاه برای واگذاری CPU
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    // پیکربندی ثابت: بیشترین رنج و بیشترین نرخ
    const uint8_t           addr = ADS111X_ADDR_GND;        // در صورت متفاوت‌بودن، تغییر بده
    const ads111x_gain_t    gain = ADS111X_GAIN_6V144;      // ±6.144V (بیشترین رنج، کمترین گین)
    const ads111x_data_rate_t dr = DR_MAX;                  // 860 SPS

    esp_err_t err = ads1115_init(&g_adc, I2C_PORT, I2C_SDA_GPIO, I2C_SCL_GPIO,
                                 addr, gain, dr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADS1115 init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "ADS1115 ready: range=±6.144V, DR=860SPS, I2C=400kHz (NG)");

    // شروع تسک خواندن
    xTaskCreate(ads_reader_task, "ads_reader", 3072, NULL, 5, NULL);
}
