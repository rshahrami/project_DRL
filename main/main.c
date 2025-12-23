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

const int uart_buffer_size = 4096;

double epsilon = 1e-9;

// ------------------- نوع داده نمونه ADC -------------------
typedef struct {
    float com_volt;
    float diff_volt;
} adc_sample_t;

typedef struct {
    int32_t ch1;
    int32_t ch2;
} adc_raw_sample_t;

// ------------------- Queue global برای اشتراک داده -------------------
static QueueHandle_t sample_queue = NULL;

static QueueHandle_t raw_queue = NULL;

// ------------------- تبدیل ADC به ولت -------------------
// float ads131_convert_to_volt(int32_t adc_code, float vref) {
//     float vdiff = ((float)adc_code / 8388607.0f) * (2.0f * vref);
//     return vdiff + vref; // بازگرداندن به بازه 0 تا 3.3
// }


// float ads131_convert_to_volt(int32_t adc_code, float vref) {
//     return ((float)adc_code / 8388607.0f) * vref;
// }


float ads131_convert_to_volt(int32_t code) {
    float vdiff = ((float)code / 8388607.0f) * 1.65f;
    return vdiff * 2.0f;   // بازگردانی به ±3V
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

    ads131_init(&adc_dev, SPI2_HOST,
                15, 16, 14, GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_11,
                SPI_FREQ_HZ);

    ads131_set_data_rate(&adc_dev, ADS131_RATE_7812SPS);

    while (1) {
        if (ads131_wait_drdy(&adc_dev, portMAX_DELAY)) {
            if (ads131_read_frame(&adc_dev, &frame) == ESP_OK) {
                adc_raw_sample_t sample = {
                    .ch1 = frame.ch[1],
                    .ch2 = frame.ch[2]
                };

                // اضافه کردن به Queue، اگر پر بود، قدیمی‌ترین نمونه حذف شود
                if (xQueueSend(raw_queue, &sample, 0) != pdTRUE) {
                    adc_raw_sample_t tmp;
                    xQueueReceive(raw_queue, &tmp, 0);
                    xQueueSend(raw_queue, &sample, 0);
                }
            }
        }
    }
}


// void reader_task(void *arg) {
//     ads131_t adc_dev;
//     ads131_frame_t frame;
//     adc_sample_t sample;

//     // مقداردهی اولیه ADC
//     ads131_init(
//         &adc_dev, SPI2_HOST,
//         15, 16, 14,       // MISO, MOSI, SCLK
//         GPIO_NUM_12,       // CS
//         GPIO_NUM_13,       // DRDY
//         GPIO_NUM_11,       // RESET
//         SPI_FREQ_HZ
//     );

//     ads131_set_data_rate(&adc_dev, ADS131_RATE_7812SPS);

//     while (1) {
//         if (ads131_wait_drdy(&adc_dev, portMAX_DELAY)) {
//             if (ads131_read_frame(&adc_dev, &frame) == ESP_OK) {


//                 int32_t raw_ch1 = frame.ch[1];
//                 int32_t raw_ch2 = frame.ch[2];


//                 // sample.com_volt = ads131_convert_to_volt(frame.ch[1], 1.65f);
//                 // sample.diff_volt = ads131_convert_to_volt(frame.ch[2], 1.65f);

//                 // sample.com_volt = ads131_convert_to_volt(frame.ch[1]);
//                 // sample.diff_volt = ads131_convert_to_volt(frame.ch[2]);

//                 printf("%ld,%ld\n", raw_ch1, raw_ch2);
//                 // اضافه کردن نمونه به Queue، اگر پر بود، قدیمی‌ترین نمونه حذف می‌شود
//                 if (xQueueSend(sample_queue, &sample, 0) != pdTRUE) {
//                     adc_sample_t tmp;
//                     xQueueReceive(sample_queue, &tmp, 0);
//                     xQueueSend(sample_queue, &sample, 0);
//                 }
//             } else {
//                 ESP_LOGW(TAG, "Failed to read ADC frame");
//             }
//         }
//     }
// }

// ------------------- Task پردازش نمونه ها -------------------
// void process_task(void *arg) {
//     adc_sample_t s;

//     while (1) {
//         if (xQueueReceive(sample_queue, &s, portMAX_DELAY)) {
//             float com_volt = s.com_volt;
//             float diff_volt = s.diff_volt;

//             // printf("%.4f,%.4f\n", com_volt, diff_volt);


//         }
//     }
// }



// void process_raw_task(void *arg) {
//     adc_raw_sample_t s;
//     static int cnt = 0;

//     while (1) {
//         if (xQueueReceive(raw_queue, &s, portMAX_DELAY)) {

//             printf("%ld,%ld\n", s.ch1, s.ch2);

//         }
//     }
// }


void process_raw_task_binary(void *arg) {
    adc_raw_sample_t s;

    while (1) {
        if (xQueueReceive(raw_queue, &s, portMAX_DELAY)) {
            // ارسال مستقیم باینری به UART
            uart_write_bytes(UART_NUM_0, (const char*)&s, sizeof(s));
        }
    }
}


/*
    921600
    115200
    9600 


*/

void set_uart_baud(void) {
    uart_set_baudrate(UART_NUM_0, 921600);
}


// ------------------- Main -------------------
// ------------------- Main -------------------
// ------------------- Main -------------------
void app_main(void) {

    set_uart_baud();

    // نصب driver UART با بافر TX بزرگ
    uart_config_t uart_config = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));

    // RX buffer حداقل 256، TX buffer = uart_buffer_size
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, uart_buffer_size, 0, NULL, 0));


    // ایجاد Queue قبل از هر Task
    // sample_queue = xQueueCreate(256, sizeof(adc_sample_t));
    // if (!sample_queue) {
    //     ESP_LOGE(TAG, "sample_queue creation failed");
    //     return;
    // }

    raw_queue = xQueueCreate(256, sizeof(adc_raw_sample_t));
    if (!raw_queue) {
        ESP_LOGE(TAG, "raw_queue creation failed");
        return;
    }

    xTaskCreate(rmt_task, "rmt_task", 2048, NULL, 10, NULL);
    xTaskCreate(reader_task, "reader_task", 4096, NULL, 5, NULL);
    // xTaskCreate(process_task, "process_task", 4096, NULL, 5, NULL);
    xTaskCreate(process_raw_task_binary, "process_raw_task_binary", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "All tasks started");
}

