#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "ads131.h"
#include "max_value.h"
#include "phase_detect.h"
#include "phase_shifter.h"

// #include "esp_console.h"
#include "driver/uart.h"

#define RMT_TX_GPIO 17
#define SPI_FREQ_HZ 2000000

static const char *TAG = "main";
double epsilon = 1e-9;

// ------------------- نوع داده نمونه ADC -------------------
typedef struct {
    float com_volt;
    float diff_volt;
} adc_sample_t;

// ------------------- Queue global برای اشتراک داده -------------------
static QueueHandle_t sample_queue = NULL;

// ------------------- تبدیل ADC به ولت -------------------
float ads131_convert_to_volt(int32_t adc_code, float vref) {
    float vdiff = ((float)adc_code / 8388607.0f) * (2.0f * vref);
    return vdiff + vref; // بازگرداندن به بازه 0 تا 3.3
}

// ------------------- Task تولید سیگنال RMT -------------------
void rmt_task(void *arg) {
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_CHANNEL_0,
        .gpio_num = RMT_TX_GPIO,
        .clk_div = 10, // 80MHz / 10 = 8MHz tick => 125ns
        .mem_block_num = 1,
        .tx_config.loop_en = true,
        .tx_config.carrier_en = false,
        .tx_config.idle_output_en = true,
        .tx_config.idle_level = RMT_IDLE_LEVEL_LOW
    };

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    rmt_item32_t item = {
        .level0 = 1,
        .duration0 = 2, // 2 * 125ns = 250ns (نمونه)
        .level1 = 0,
        .duration1 = 2
    };

    while (1) {
        ESP_ERROR_CHECK(rmt_write_items(config.channel, &item, 1, true));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ------------------- Task خواندن ADS131 -------------------
void reader_task(void *arg) {
    ads131_t adc_dev;
    ads131_frame_t frame;
    adc_sample_t sample;

    // مقداردهی اولیه ADC
    ads131_init(
        &adc_dev, SPI2_HOST,
        15, 16, 14,       // MISO, MOSI, SCLK
        GPIO_NUM_12,       // CS
        GPIO_NUM_13,       // DRDY
        GPIO_NUM_11,       // RESET
        SPI_FREQ_HZ
    );

    ads131_set_data_rate(&adc_dev, ADS131_RATE_7812SPS);

    while (1) {
        if (ads131_wait_drdy(&adc_dev, portMAX_DELAY)) {
            if (ads131_read_frame(&adc_dev, &frame) == ESP_OK) {
                sample.com_volt = ads131_convert_to_volt(frame.ch[1], 1.65f);
                sample.diff_volt = ads131_convert_to_volt(frame.ch[2], 1.65f);

                // اضافه کردن نمونه به Queue، اگر پر بود، قدیمی‌ترین نمونه حذف می‌شود
                if (xQueueSend(sample_queue, &sample, 0) != pdTRUE) {
                    adc_sample_t tmp;
                    xQueueReceive(sample_queue, &tmp, 0);
                    xQueueSend(sample_queue, &sample, 0);
                }
            } else {
                ESP_LOGW(TAG, "Failed to read ADC frame");
            }
        }
    }
}

// ------------------- Task پردازش نمونه ها -------------------
void process_task(void *arg) {
    adc_sample_t s;

    while (1) {
        if (xQueueReceive(sample_queue, &s, portMAX_DELAY)) {
            float com_volt = s.com_volt;
            float diff_volt = s.diff_volt;

            // پردازش یا محاسبات دلخواه
            // ESP_LOGI(TAG, "COM=%.3f, DIFF=%.3f", com_volt, diff_volt);
            printf("%.4f,%.4f\n", com_volt, diff_volt);

            // ESP_LOGI(TAG, "COM:%d, DIFF:%d", com_volt, diff_volt);
            // لاگ نمونه‌ها با نرخ پایین‌تر برای جلوگیری از flood
            // static int cnt = 0;
            // if (++cnt % 50 == 0) {
            //     ESP_LOGI(TAG, "COM=%.3f V, DIFF=%.3f V, PHASE=%.3f deg", com_volt, diff_volt, phase);
            // }
        }
    }
}


void set_uart_baud(void) {
    uart_set_baudrate(UART_NUM_0, 921600);
}


// ------------------- Main -------------------
void app_main(void) {

    set_uart_baud();
    // ایجاد Queue قبل از هر Task
    sample_queue = xQueueCreate(256, sizeof(adc_sample_t));
    if (!sample_queue) {
        ESP_LOGE(TAG, "Queue creation failed");
        return;
    }

    xTaskCreate(rmt_task, "rmt_task", 2048, NULL, 10, NULL);
    xTaskCreate(reader_task, "reader_task", 4096, NULL, 5, NULL);
    xTaskCreate(process_task, "process_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "All tasks started");
}
