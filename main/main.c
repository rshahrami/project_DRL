







/////////////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "ads131.h"

#define RMT_TX_GPIO 17
static const char *TAG = "main";

void rmt_task(void *arg)
{
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_CHANNEL_0,
        .gpio_num = RMT_TX_GPIO,
        .clk_div = 10, // APB 80MHz / 10 = 8MHz tick => 125ns
        .mem_block_num = 1,
        .tx_config.loop_en = true,
        .tx_config.carrier_en = false,
        .tx_config.idle_output_en = true,
        .tx_config.idle_level = RMT_IDLE_LEVEL_LOW
    };

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // یک آیتم برای ۲ مگاهرتز (500ns high + 500ns low)
    rmt_item32_t item;
    item.level0 = 1;
    item.duration0 = 2; // 4 * 125ns = 500ns
    item.level1 = 0;
    item.duration1 = 2;

    while (1) {
        ESP_ERROR_CHECK(rmt_write_items(config.channel, &item, 1, true));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void reader_task(void *arg)
{
    ads131_t dev;

    int miso_pin = 15;
    int mosi_pin = 16;
    int sclk_pin = 14;
    gpio_num_t cs_pin = GPIO_NUM_12;
    gpio_num_t drdy_pin = GPIO_NUM_13;
    gpio_num_t rst_pin = GPIO_NUM_11;
    spi_host_device_t host = SPI2_HOST;
    int spi_freq_hz = 2000000;

    if (ads131_init(&dev, host, miso_pin, mosi_pin, sclk_pin,
                     cs_pin, drdy_pin, rst_pin, spi_freq_hz) != ESP_OK) {
        ESP_LOGE(TAG, "ADS131 init failed");
        vTaskDelete(NULL);
        return;
    }

    int32_t samples[4];

    while (1) {
        if (ads131_wait_drdy(&dev, pdMS_TO_TICKS(1000))) {
            if (ads131_read_frame_raw(&dev, samples, 4) == ESP_OK) {
                ESP_LOGI(TAG, "S: %d %d %d %d", samples[0], samples[1], samples[2], samples[3]);
            } else {
                ESP_LOGE(TAG, "Read frame failed");
            }
        }
    }
}

void app_main(void)
{
    xTaskCreatePinnedToCore(rmt_task, "rmt_task", 2048, NULL, 10, NULL, 0);
    xTaskCreate(reader_task, "reader", 4096, NULL, 5, NULL);
}
