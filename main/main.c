#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "timer.h"
#include "phase_detect.c"
#include "phase_shifter.c"
#include "max_value.h"
#include "ads1115_oneshot.h"
#include "i2cdev.h"   // فقط برای i2cdev_init و ساخت دیسکریپتور (نه درایور قدیمی)

static const char *TAG = "main";

#define PI 3.14159265358979323846

#ifndef SAMPLE_RATE_HZ
#define SAMPLE_RATE_HZ 1000.0f
#endif

#define TABLE_SIZE 1000

// پین‌های I2C برای ESP32-S3 (در صورت نیاز تغییر بده)
#define I2C_PORT     I2C_NUM_0
#define I2C_SDA_GPIO 18
#define I2C_SCL_GPIO 19

PeakTracker signalDiffTracker, signalCommonTracker;

// ----- فیلتر باندباریک 50 هرتز -----
typedef struct {
    float prev_input[2];
    float prev_output[2];
} Bandpass50Hz;

static inline void initBandpass(Bandpass50Hz* bp) {
    bp->prev_input[0]  = 0.0f;
    bp->prev_input[1]  = 0.0f;
    bp->prev_output[0] = 0.0f;
    bp->prev_output[1] = 0.0f;
}

// Fs=1000 Hz, f0=50 Hz, Q≈10 (RBJ bandpass, a0=1)
static const float b0 = +0.0152157534f;
static const float b1 =  0.0f;
static const float b2 = -0.0152157534f;
static const float a1 = -1.8731709497f;
static const float a2 = +0.9695684932f;

static inline float filter50Hz(Bandpass50Hz* bp, float x) {
    float y = b0*x + b1*bp->prev_input[0] + b2*bp->prev_input[1]
                    - a1*bp->prev_output[0] - a2*bp->prev_output[1];
    bp->prev_input[1]  = bp->prev_input[0];
    bp->prev_input[0]  = x;
    bp->prev_output[1] = bp->prev_output[0];
    bp->prev_output[0] = y;
    return y;
}

static inline void init_trackers(void) {
    initPeakTracker(&signalDiffTracker);
    initPeakTracker(&signalCommonTracker);
}

// ----- تولید جدول سینوسی -----
static float  sine_table[TABLE_SIZE];
static double angle_1hz  = 0.0;
static double angle_50hz = 0.0;
static const double PHASE_STEP_1HZ  = 2.0 * PI *   1.0 / SAMPLE_RATE_HZ;
static const double PHASE_STEP_50HZ = 2.0 * PI *  50.0 / SAMPLE_RATE_HZ;
// نام ثابت قبلی مانده ولی مقدار 37 درجه است
static const double PHASE_SHIFT_30DEG = (37.0 / 360.0) * 2.0 * PI;

static void create_sine_table(void) {
    for (int i = 0; i < TABLE_SIZE; ++i)
        sine_table[i] = sinf((2.0f * PI * i) / TABLE_SIZE);
}

void app_main(void) {
    create_sine_table();

    gptimer_handle_t timer;
    timer_init_hz(SAMPLE_RATE_HZ, &timer, 0); // تایمر 1kHz

    Bandpass50Hz bp;
    initBandpass(&bp);

    init_trackers();
    init_phase_shifter(); // آل‌پس برای f0=50Hz و Fs فعلی

    // ----- ADS1115 -----
    ads1115_ctx_t adc = {0};
    uint8_t addr = ADS111X_ADDR_GND;                   // اگر ADDR به GND است
    ads111x_gain_t      gain = ADS111X_GAIN_4V096;     // ±4.096V
    ads111x_data_rate_t dr   = ADS111X_DATA_RATE_860;  // 860 SPS

    esp_err_t err = ads1115_init(&adc, I2C_PORT, I2C_SDA_GPIO, I2C_SCL_GPIO, addr, gain, dr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADS1115 init failed! err=%s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "ADS1115 initialized OK at 400 kHz (i2cdev NG)");

    // حلقهٔ اصلی
    while (1) {
        if (xSemaphoreTake(timer_semaphore, portMAX_DELAY) == pdTRUE) {
            int idx_1hz  = (int)((angle_1hz  / (2.0 * PI)) * TABLE_SIZE) % TABLE_SIZE;
            int idx_50hz = (int)((angle_50hz / (2.0 * PI)) * TABLE_SIZE) % TABLE_SIZE;

            int idx_1hz_shifted  = (int)(((angle_1hz  + PHASE_SHIFT_30DEG) / (2.0 * PI)) * TABLE_SIZE) % TABLE_SIZE;
            int idx_50hz_shifted = (int)(((angle_50hz + PHASE_SHIFT_30DEG) / (2.0 * PI)) * TABLE_SIZE) % TABLE_SIZE;

            float sample_1hz          = sine_table[idx_1hz];
            float sample_50hz         = sine_table[idx_50hz];
            float sample_1hz_shifted  = sine_table[idx_1hz_shifted];
            float sample_50hz_shifted = sine_table[idx_50hz_shifted];

            float combined_signal_1hz_50hz         = (0.01f * sample_1hz) + sample_50hz;
            float combined_signal_1hz_50hz_shifted = 0.9f * ((-1.0f * (0.01f * sample_1hz_shifted)) + sample_50hz_shifted);

            float diff   = combined_signal_1hz_50hz_shifted - combined_signal_1hz_50hz;
            float common = 0.5f * (combined_signal_1hz_50hz_shifted + combined_signal_1hz_50hz);

            float phase_diff_common_ref = calculate_phase(diff, common);
            set_phase_shift_degrees(phase_diff_common_ref);
            float common_shift = process_sample(common);

            float filtered_50_diff = filter50Hz(&bp, diff);

            float max_diff   = updatePeak(&signalDiffTracker, filtered_50_diff);
            float max_common = updatePeak(&signalCommonTracker, common_shift);

            float common_ref = common_shift * (max_diff / (max_common + 1e-12f));
            float out = common_ref + diff;

            ESP_LOGI(TAG, "sig=%.3f | shifted_sig=%.3f", out, diff);

            angle_1hz  += PHASE_STEP_1HZ;
            angle_50hz += PHASE_STEP_50HZ;
            if (angle_1hz  >= 2.0 * PI) angle_1hz  -= 2.0 * PI;
            if (angle_50hz >= 2.0 * PI) angle_50hz -= 2.0 * PI;

            vTaskDelay(1);
        }
    }
}
