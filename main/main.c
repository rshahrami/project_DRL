#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/uart.h"
#include "driver/ledc.h"

#include "ads131.h"
#include "lpf_biquad.h"
#include "anc_iq_biquad.h"

/* ================== تنظیمات ================== */

#define TAG     "ADS131_APP"

/* ---- ADC CLKIN (از ESP32-S3) ---- */
#define ADC_CLKIN_GPIO   17
#define ADC_CLKIN_HZ     2048000   // 2.048 MHz

/* ---- SPI ---- */
#define SPI_FREQ_HZ      8000000

/* ---- Sample rate ---- */
#define TARGET_SPS       1000

/* ---- Ring buffer ---- */
#define RB_SIZE          4096      // حتماً توان 2 باشد

/* ================== نوع داده ================== */

typedef struct {
    int32_t ch1;
    int32_t ch2;
} adc_raw_sample_t;

/* ================== Ring buffer ================== */

static adc_raw_sample_t rb[RB_SIZE];
static volatile uint32_t rb_w = 0;
static volatile uint32_t rb_r = 0;

static inline uint32_t rb_next(uint32_t i)
{
    return (i + 1) & (RB_SIZE - 1);
}

static inline bool rb_empty(void)
{
    return rb_r == rb_w;
}

/* ================== DSP ================== */

static anc_iq_t anc;
static lpf4_t   lpf_clean;

/* ================== ADC → mV ================== */

static inline float ads131_convert_to_mV(int32_t code)
{
    return ((float)code / 8388607.0f) * 1200.0f;
}

/* ================== CLKIN ================== */

static void ads131_start_clkin(void)
{
    ledc_timer_config_t timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_1_BIT,
        .freq_hz          = ADC_CLKIN_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .gpio_num   = ADC_CLKIN_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 1,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));

    ESP_LOGI(TAG, "CLKIN started @ %d Hz", ADC_CLKIN_HZ);
}

/* ================== reader task ================== */

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
    ads131_set_data_rate(&adc_dev, ADS131_RATE_1KSPS);

    ESP_LOGI(TAG, "ADS131 sampling started (%d SPS)", TARGET_SPS);

    while (1) {
        if (ads131_wait_drdy(&adc_dev, portMAX_DELAY)) {
            if (ads131_read_frame(&adc_dev, &frame) == ESP_OK) {

                uint32_t nw = rb_next(rb_w);
                if (nw == rb_r) {
                    // buffer full → قدیمی‌ترین نمونه دور انداخته می‌شود
                    rb_r = rb_next(rb_r);
                }

                rb[rb_w].ch1 = frame.ch[1];
                rb[rb_w].ch2 = frame.ch[2];
                rb_w = nw;
            }
        }
    }
}

/* ================== process task ================== */

void process_task(void *arg)
{
    const float fs = 1000.0f;

    anc_iq_init(&anc, fs);
    anc.w_max    = 3.0f;
    anc.com_lim  = 500.0f;
    anc.diff_sat = 1100.0f;
    anc.com_sat  = 1100.0f;
    anc.leak     = 0.0010f;
    anc.mu       = 0.03f;

    lpf4_init(&lpf_clean, fs, 100.0f);

    const uint32_t warmup_samp  = (uint32_t)(0.5f * fs);   // 0.5s
    const uint32_t capture_samp = 3000;                    // 3s واقعی
    const uint32_t K = 5;                                  // ✅ تا 100Hz

    static uint32_t n = 0;
    bool capturing = false;
    uint32_t cap_count = 0;       // شمارنده داخل پنجره
    uint32_t window_id = 0;

    while (1) {
        if (rb_empty()) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }

        adc_raw_sample_t s = rb[rb_r];
        rb_r = rb_next(rb_r);

        float com  = ads131_convert_to_mV(s.ch1);
        float diff = ads131_convert_to_mV(s.ch2);

        float clean    = anc_iq_process(&anc, com, diff);
        float clean_lp = lpf4_process(&lpf_clean, clean);

        uint32_t cur_n = n++;

        // warmup فقط یک بار در شروع
        if (!capturing) {
            if (cur_n < warmup_samp) continue;
            capturing = true;
            cap_count = 0;
            window_id = 0;
            printf("#BEGIN,%lu\n", (unsigned long)window_id);
        }

        // داخل پنجره 3 ثانیه‌ای
        if (cap_count < capture_samp) {
            if ((cur_n % K) == 0) {
                printf("%lu,%.3f,%.3f,%.3f\n",
                       (unsigned long)cur_n, com, diff, clean_lp);

                // کمک به WDT و UART (خیلی مهم)
                if ((cap_count & 0x3F) == 0) vTaskDelay(pdMS_TO_TICKS(1)); // هر 64 نمونه واقعی
            }
            cap_count++;
        } else {
            // پنجره تمام شد
            printf("#END,%lu\n", (unsigned long)window_id);

            window_id++;
            cap_count = 0;

            // یک مکث کوچک بین پنجره‌ها (هم برای WDT هم برای اینکه PC عقب‌افتادگی نگیرد)
            vTaskDelay(pdMS_TO_TICKS(50));

            // شروع پنجره بعدی
            printf("#BEGIN,%lu\n", (unsigned long)window_id);
        }
    }
}


// void process_task(void *arg)
// {
//     const float fs = 1000.0f;

//     anc_iq_init(&anc, fs);
//     anc.w_max    = 3.0f;
//     anc.com_lim  = 500.0f;
//     anc.diff_sat = 1100.0f;
//     anc.com_sat  = 1100.0f;
//     anc.leak     = 0.0010f;
//     anc.mu       = 0.03f;

//     lpf4_init(&lpf_clean, fs, 100.0f);

//     const uint32_t warmup_samp = (uint32_t)(0.5f * fs);   // 0.5s warmup
//     const uint32_t capture_samp = 3000;                   // 3s = 3 سیکل 1Hz
//     const uint32_t K = 5;                                // decimate (برای 1Hz عالیه)

//     static uint32_t n = 0;
//     uint32_t printed_window_done = 0;  // تعداد نمونه‌های واقعی که از شروع capture شمرده‌ایم
//     bool capturing = false;

//     while (1) {
//         if (rb_empty()) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }

//         adc_raw_sample_t s = rb[rb_r];
//         rb_r = rb_next(rb_r);

//         float com  = ads131_convert_to_mV(s.ch1);
//         float diff = ads131_convert_to_mV(s.ch2);

//         float clean    = anc_iq_process(&anc, com, diff);
//         float clean_lp = lpf4_process(&lpf_clean, clean);

//         uint32_t cur_n = n++;

//         // هنوز warmup تمام نشده
//         if (!capturing) {
//             if (cur_n >= warmup_samp) {
//                 capturing = true;
//                 printed_window_done = 0;
//             } else {
//                 continue;
//             }
//         }

//         // توی بازه‌ی capture هستیم
//         if (printed_window_done < capture_samp) {
//             if ((cur_n % K) == 0) {
//                 printf("%lu,%.3f,%.3f,%.3f\n",
//                        (unsigned long)cur_n, com, diff, clean_lp);

//                 // ✅ خیلی مهم برای جلوگیری از WDT و روان شدن UART
//                 vTaskDelay(pdMS_TO_TICKS(1));
//             }
//             printed_window_done++;
//         } else {
//             // ✅ کار تموم: دیگه چاپ نکن
//             ESP_LOGI(TAG, "Capture done: %lu samples (after warmup).", (unsigned long)capture_samp);

//             // اگر می‌خوای کل کار همینجا متوقف شه:
//             vTaskSuspend(NULL);
//             // یا اگر می‌خوای فقط چاپ قطع شه ولی پردازش ادامه پیدا کنه:
//             // vTaskDelay(pdMS_TO_TICKS(1000));
//         }
//     }
// }


/* ================== UART ================== */

static void set_uart_baud(void)
{
    uart_set_baudrate(UART_NUM_0, 921600);
}

/* ================== main ================== */

void app_main(void)
{
    set_uart_baud();
    ads131_start_clkin();

    // xTaskCreate(reader_task,  "reader_task",  4096, NULL, 8, NULL);
    // xTaskCreate(process_task, "process_task", 4096, NULL, 4, NULL);

    xTaskCreatePinnedToCore(reader_task,  "reader_task",  4096, NULL, 8, NULL, 0);
    xTaskCreatePinnedToCore(process_task, "process_task", 4096, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "System started");
}
