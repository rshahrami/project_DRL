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

static anc_t    anc;
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

    anc_init(&anc, fs);

    /* اگر خواستی دستی تیون کنی */
    anc.mu      = 0.05f;     // پایدارتر از 0.10
    anc.ref_lim = 1200.0f;
    anc.leak    = 0.0005f;

    /* Q کمتر => تحمل لغزش بیشتر */
    anc_bandpass_init(&anc, fs, 50.0f, 1.3f);

    lpf4_init(&lpf_clean, fs, 100.0f);

    const uint32_t warmup_samp  = (uint32_t)(0.5f * fs);   // 0.5s
    const uint32_t capture_samp = 3000;                    // 3s واقعی
    const uint32_t K = 5;                                  // ✅ تا 100Hz

    static uint32_t n = 0;
    bool capturing = false;
    uint32_t cap_count = 0;
    uint32_t window_id = 0;

    while (1) {
        if (rb_empty()) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }

        adc_raw_sample_t s = rb[rb_r];
        rb_r = rb_next(rb_r);

        float com  = ads131_convert_to_mV(s.ch1);
        float diff = ads131_convert_to_mV(s.ch2);

        /* ===== ANC Path (فقط 50Hz) ===== */
        float com_bp  = anc_ref_bp_process(&anc, com);
        float diff_bp = anc_d_bp_process(&anc, diff);

        if (com_bp >  anc.ref_lim) com_bp =  anc.ref_lim;
        if (com_bp < -anc.ref_lim) com_bp = -anc.ref_lim;

        float noise_est = anc_fir_nlms(&anc, com_bp, diff_bp);

        /* حذف از سیگنال اصلی */
        float clean    = diff - noise_est;
        float clean_lp = lpf4_process(&lpf_clean, clean);

        uint32_t cur_n = n++;

        if (!capturing) {
            if (cur_n < warmup_samp) continue;
            capturing = true;
            cap_count = 0;
            window_id = 0;
            printf("#BEGIN,%lu\n", (unsigned long)window_id);
        }

        if (cap_count < capture_samp) {
            if ((cur_n % K) == 0) {
                printf("%lu,%.3f,%.3f,%.3f\n",
                       (unsigned long)cur_n, com, diff, clean_lp);

                if ((cap_count & 0x3F) == 0) vTaskDelay(pdMS_TO_TICKS(1));
            }
            cap_count++;
        } else {
            printf("#END,%lu\n", (unsigned long)window_id);

            window_id++;
            cap_count = 0;

            vTaskDelay(pdMS_TO_TICKS(50));

            printf("#BEGIN,%lu\n", (unsigned long)window_id);
        }
    }
}

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

    xTaskCreatePinnedToCore(reader_task,  "reader_task",  4096, NULL, 8, NULL, 0);
    xTaskCreatePinnedToCore(process_task, "process_task", 4096, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "System started");
}
