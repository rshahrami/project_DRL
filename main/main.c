#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/uart.h"
#include "driver/rmt.h"

#include "ads131.h"
#include "max_value.h"


/* ================== تنظیمات ================== */

#define TAG               "ADS131_APP"

#define SPI_FREQ_HZ       2000000
#define RMT_TX_GPIO       17

#define BUF_SIZE          1000   // ~2.1 ثانیه @ 7812 SPS


static PeakTracker peak_ch1;
static PeakTracker peak_ch2;
/* ================== نوع داده ================== */

typedef struct {
    int32_t ch1;
    int32_t ch2;
} adc_raw_sample_t;

/* ================== بافر سراسری ================== */

static adc_raw_sample_t sample_buf[BUF_SIZE];
static volatile uint32_t write_idx   = 0;
static volatile bool buffer_full     = false;

/* ================== تبدیل ADC به ولت ================== */
// Gain = 1
// Vref = 1.2V
// Full-scale = ±1.2V
float ads131_convert_to_volt(int32_t code)
{
    return ((float)code / 8388607.0f) * 1200.0f;
}

/* ================== RMT task (کم‌اهمیت) ================== */

void rmt_task(void *arg)
{
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_CHANNEL_0,
        .gpio_num = RMT_TX_GPIO,
        .clk_div = 10,
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
        .duration0 = 2,
        .level1 = 0,
        .duration1 = 2
    };

    while (1) {
        rmt_write_items(config.channel, &item, 1, true);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ================== reader task (حیاتی) ================== */

void reader_task(void *arg)
{
    ads131_t adc_dev;
    ads131_frame_t frame;

    ads131_init(&adc_dev,
                SPI2_HOST,
                15, 16, 14,
                GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_11,
                SPI_FREQ_HZ);

    ads131_set_gain_1_all(&adc_dev);
    ads131_set_data_rate(&adc_dev, ADS131_RATE_7812SPS);

    ESP_LOGI(TAG, "ADS131 sampling started");

    while (1) {
        if (ads131_wait_drdy(&adc_dev, portMAX_DELAY)) {

            if (ads131_read_frame(&adc_dev, &frame) == ESP_OK) {

                if (!buffer_full) {
                    sample_buf[write_idx].ch1 = frame.ch[1];
                    sample_buf[write_idx].ch2 = frame.ch[2];

                    write_idx++;

                    if (write_idx >= BUF_SIZE) {
                        buffer_full = true;
                        ESP_LOGI(TAG, "Buffer full (%d samples)", BUF_SIZE);
                    }
                }
            }
        }
    }
}

/* ================== process task (تبدیل + ارسال) ================== */

void process_task(void *arg)
{
    while (1) {

        /* صبر کن تا کل دیتاست جمع شود */
        if (!buffer_full) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        ESP_LOGI(TAG, "Sending data...");

        initPeakTracker(&peak_ch1);
        initPeakTracker(&peak_ch2);


        for (uint32_t i = 0; i < BUF_SIZE; i++) {
            float com = ads131_convert_to_volt(sample_buf[i].ch1);
            float diff = ads131_convert_to_volt(sample_buf[i].ch2);


            float max_com = updatePeak50Hz(&peak_ch1, com);
            float max_diff = updatePeak50Hz(&peak_ch2, diff);

            float com_amp = (max_diff/max_com) * com;

            // printf("%.4f,%.4f\n", v1, v2);
            printf("%.4f,%.4f\n", com_amp, diff);
        }

        /* ریست برای برداشت بعدی */
        write_idx   = 0;
        buffer_full = false;

        ESP_LOGI(TAG, "Transmission done");
    }
}

void set_uart_baud(void) {
    uart_set_baudrate(UART_NUM_0, 921600);
}


/* ================== main ================== */

void app_main(void)
{
    /* اولویت‌ها:
       reader  = 8  (ADC real-time)
       process = 4
       rmt     = 2
    */
    set_uart_baud();

    // initPeakTracker(&peak_ch1);
    // initPeakTracker(&peak_ch2);

    xTaskCreate(reader_task,  "reader_task",  4096, NULL, 8, NULL);
    xTaskCreate(process_task, "process_task", 4096, NULL, 4, NULL);
    xTaskCreate(rmt_task,     "rmt_task",     2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "System started");
}
