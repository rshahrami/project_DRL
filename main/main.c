#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/uart.h"
#include "driver/ledc.h"

#include "ads131.h"
#include "max_value.h"
#include "iq_subtract_com.h"
#include "iq_subtract_nco.h"

// #include "anc_nlms.h"

// #include "anc_fir_nlms.h"
#include "lpf_biquad.h"


#include "anc_iq_biquad.h"

/* ================== تنظیمات ================== */

#define TAG                 "ADS131_APP"

/* ---- ADC CLKIN (از ESP32-S3) ---- */
#define ADC_CLKIN_GPIO   17
#define ADC_CLKIN_HZ     2048000   // پیشنهاد: 2.048MHz (پایدارتر از 4.096 روی LEDC)

/* ---- SPI فقط برای خواندن فریم ---- */
#define SPI_FREQ_HZ         8000000    // 8 MHz (امن و سریع برای 4kSPS)

/* ---- نرخ نمونه برداری هدف ----
   با CLKIN=4.096MHz و OSR=512 => 4000 SPS
*/
#define TARGET_SPS          4000

/* ~2 ثانیه دیتـا */
#define BUF_SIZE            (TARGET_SPS * 5)

#define LPF_ALPHA           0.005f     // 0.05 خیلی تند بود؛ این ملایم‌تره

// static Cancel50 c50;

// static anc2_t anc;
// static anc_fir_t anc;
static anc_iq_t anc;  
static lpf4_t lpf_clean;


// static PeakTracker peak_ch1;
// static PeakTracker peak_ch2;

/* ================== نوع داده ================== */

typedef struct {
    int32_t ch1;
    int32_t ch2;
} adc_raw_sample_t;

/* ================== بافر سراسری ================== */

static adc_raw_sample_t sample_buf[BUF_SIZE];
static volatile uint32_t write_idx   = 0;
static volatile bool buffer_full     = false;

/* ================== تبدیل ADC به mV ==================
   Gain=1, Vref=1.2V, Full-scale=±1.2V
*/
static inline float ads131_convert_to_mV(int32_t code)
{
    return ((float)code / 8388607.0f) * 1200.0f; // mV
}

/* ================== تولید CLKIN با LEDC ================== */

static void ads131_start_clkin(void)
{
    ledc_timer_config_t timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,   // <-- S3
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_1_BIT,      // برای فرکانس بالا
        .freq_hz          = ADC_CLKIN_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .gpio_num   = ADC_CLKIN_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,         // <-- S3
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 1,                           // 50% در 1-bit
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));

    ESP_LOGI(TAG, "CLKIN started on GPIO%d @ %d Hz (LEDC low-speed)", ADC_CLKIN_GPIO, ADC_CLKIN_HZ);
}

/* ================== reader task (حیاتی) ================== */

void reader_task(void *arg)
{
    ads131_t adc_dev;
    ads131_frame_t frame;

    ads131_init(&adc_dev,
                SPI2_HOST,
                15, 16, 14,                 // MISO, MOSI, SCLK (SPI)
                GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_11,   // CS, DRDY, RESET
                SPI_FREQ_HZ);

    ads131_set_gain_1_all(&adc_dev);

    // این باید در درایور، CLOCK reg را درست تنظیم کند (OSR/PWR)
    // اینجا ما هدفمان 4kSPS است
    ads131_set_data_rate(&adc_dev, ADS131_RATE_4KSPS);

    ESP_LOGI(TAG, "ADS131 sampling started (target %d SPS)", TARGET_SPS);

    // برای sanity-check می‌توانی تعداد DRDY در ثانیه را بشمری
    uint32_t drdy_count = 0;
    TickType_t t0 = xTaskGetTickCount();

    while (1) {
        if (ads131_wait_drdy(&adc_dev, portMAX_DELAY)) {
            drdy_count++;

            if ((xTaskGetTickCount() - t0) >= pdMS_TO_TICKS(1000)) {
                ESP_LOGI(TAG, "DRDY/s = %u", (unsigned)drdy_count);
                drdy_count = 0;
                t0 = xTaskGetTickCount();
            }

            if (ads131_read_frame(&adc_dev, &frame) == ESP_OK) {

                if (buffer_full) {
                    // اگر بافر پره، reader باید خیلی کوتاه عقب بکشه
                    vTaskDelay(pdMS_TO_TICKS(1));
                    continue;
                }

                sample_buf[write_idx].ch1 = frame.ch[1];
                sample_buf[write_idx].ch2 = frame.ch[2];

                write_idx++;

                if (write_idx >= BUF_SIZE) {
                    buffer_full = true;
                }
            }
        }
    }
}

/* ================== process task (تبدیل + ارسال) ================== */
// void process_task(void *arg)
// {
//     const float fs = 4000.0f;
//     anc2_init(&anc, fs, 50.0f, 0.002f);

//     while (1) {
//         if (!buffer_full) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

//         uint32_t warmup = (uint32_t)(0.5f * fs); // 0.5s

//         for (uint32_t i = 0; i < BUF_SIZE; i++) {
//             float com  = ads131_convert_to_mV(sample_buf[i].ch1);
//             float diff = ads131_convert_to_mV(sample_buf[i].ch2);

//             float clean = anc2_process(&anc, com, diff);

//             if (i == warmup) {
//                 // اگر می‌خوای بعد از قفل شدن وزن‌ها ثابت بمانند:
//                 // anc.adapt = 0;
//             }

//             if (i >= warmup) {
//                 printf("%.4f,%.4f\n", com, clean);
//             }
//         }

//         buffer_full = false;
//         write_idx = 0;
//     }
// }


void process_task(void *arg)
{
    const float fs = 4000.0f;
    
    anc_iq_init(&anc, fs);
    lpf4_init(&lpf_clean, fs, 5.0f);
    
    while (1) {
        if (!buffer_full) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

        uint32_t warmup = (uint32_t)(0.5f * fs);

        for (uint32_t i = 0; i < BUF_SIZE; i++) {
            float com  = ads131_convert_to_mV(sample_buf[i].ch1);
            float diff = ads131_convert_to_mV(sample_buf[i].ch2);

            float clean = anc_iq_process(&anc, com, diff);
            float clean_lp = lpf4_process(&lpf_clean, clean);

            if (i >= warmup) {
                printf("%.4f,%.4f,%.4f\n", com, diff, clean_lp);
            }
        }

        buffer_full = false;
        write_idx = 0;
    }
}




static void set_uart_baud(void)
{
    uart_set_baudrate(UART_NUM_0, 921600);
}

/* ================== main ================== */

void app_main(void)
{
    set_uart_baud();

    // 1) اول CLKIN را راه بینداز (GPIO17)
    ads131_start_clkin();

    // 2) بعد تسک‌ها
    xTaskCreate(reader_task,  "reader_task",  4096, NULL, 8, NULL);
    xTaskCreate(process_task, "process_task", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "System started");
}
