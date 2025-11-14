#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ads131.h"

static const char *TAG = "main";

void reader_task(void *arg)
{
    ads131_t dev;

    // ===== مثال پین‌ها — مطابق سیم‌کشی خودت تغییر بده =====
    int miso_pin = 15;
    int mosi_pin = 16;
    int sclk_pin = 14;
    gpio_num_t cs_pin   = GPIO_NUM_12;
    gpio_num_t drdy_pin = GPIO_NUM_13;
    gpio_num_t rst_pin  = GPIO_NUM_11;
    spi_host_device_t host = SPI2_HOST;
    int spi_freq_hz = 2000000; // 2 MHz
    // =====================================================

    // فراخوانی با امضای کامل (مطابق declaration در ads131.h)
    esp_err_t r = ads131_init(&dev,
                              host,
                              miso_pin, mosi_pin, sclk_pin,
                              cs_pin, drdy_pin, rst_pin,
                              spi_freq_hz);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "ads131_init failed: %d", r);
        vTaskDelete(NULL);
        return;
    }

    int32_t samples[4]; // فرض 4 کانال؛ متناسب با مدل تنظیم کن

    while (1) {
        if (ads131_wait_drdy(&dev, pdMS_TO_TICKS(1000))) {
            esp_err_t rr = ads131_read_frame_raw(&dev, samples, 4);
            if (rr == ESP_OK) {
                ESP_LOGI(TAG, "S: %d %d %d %d", samples[0], samples[1], samples[2], samples[3]);
            } else {
                ESP_LOGE(TAG, "read frame failed %d", rr);
            }
        } else {
            ESP_LOGW(TAG, "DRDY timeout");
        }
    }

    ads131_deinit(&dev);  // در پایان تسک
    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(reader_task, "reader", 4096, NULL, 5, NULL);
}

