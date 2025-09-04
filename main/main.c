#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "timer.h"
#include "phase_detect.c"
#include "phase_shifter.c"
#include "max_value.h"  // استفاده از هدر max_value.h
#include "ads1115_oneshot.h"

static const char *TAG = "main";

#define PI             3.14159265358979323846
#define SAMPLE_RATE_HZ 1000.0
#define TABLE_SIZE     1000

// پین‌های پیشنهادی برای ESP32-S3 (قابل تغییر)
#define I2C_PORT     I2C_NUM_0
#define I2C_SDA_GPIO 18
#define I2C_SCL_GPIO 19


PeakTracker signalDiffTracker, signalCommonTracker;

// فیلتر باند باریک 50 هرتز (IIR)
typedef struct {
    float prev_input[2];
    float prev_output[2];
} Bandpass50Hz;

void initBandpass(Bandpass50Hz* bp) {
    bp->prev_input[0]  = 0.0f;
    bp->prev_input[1]  = 0.0f;
    bp->prev_output[0] = 0.0f;
    bp->prev_output[1] = 0.0f;
}

// Fs = 1000 Hz, f0 = 50 Hz, Q ≈ 10 (RBJ bandpass, 0 dB peak), a0 نرمال‌شده به 1
static const float b0 = +0.0152157534f;
static const float b1 =  0.0f;
static const float b2 = -0.0152157534f;
static const float a1 = -1.8731709497f;
static const float a2 = +0.9695684932f;

float filter50Hz(Bandpass50Hz* bp, float x) {
    float y = b0*x
            + b1*bp->prev_input[0]
            + b2*bp->prev_input[1]
            - a1*bp->prev_output[0]
            - a2*bp->prev_output[1];

    bp->prev_input[1]  = bp->prev_input[0];
    bp->prev_input[0]  = x;
    bp->prev_output[1] = bp->prev_output[0];
    bp->prev_output[0] = y;

    return y;
}

// فراخوانی تابع initPeakTracker بعد از تعریف متغیرها
void init_trackers() {
    initPeakTracker(&signalDiffTracker);
    initPeakTracker(&signalCommonTracker);
}

static float sine_table[TABLE_SIZE];
static double angle_1hz  = 0.0;
static double angle_50hz = 0.0;
static const double PHASE_STEP_1HZ  = 2.0 * PI *   1.0 / SAMPLE_RATE_HZ;
static const double PHASE_STEP_50HZ = 2.0 * PI *  50.0 / SAMPLE_RATE_HZ;
// مقدار تغییر فاز 30 درجه برای هر دوره
static const double PHASE_SHIFT_30DEG = (37.0 / 360.0) * 2.0 * PI;

static void create_sine_table(void) {
    for (int i = 0; i < TABLE_SIZE; ++i) {
        sine_table[i] = sinf((2.0f * PI * i) / TABLE_SIZE);
    }
}

void app_main(void) {
    create_sine_table();

    gptimer_handle_t timer;
    timer_init_hz(SAMPLE_RATE_HZ, &timer, 0); // فقط یک تایمر برای 1kHz

    // فیلتر 50 هرتز
    Bandpass50Hz bp;
    initBandpass(&bp);

    init_trackers();
    // خیلی مهم: آل‌پس را برای f0=50Hz و Fs فعلی کالیبره کن
    init_phase_shifter();
    ///////////////////////////////////////////////////////////////////////////////
    ads1115_ctx_t adc = {0};
    // اگر پایه ADDR روی GND است:
    uint8_t addr = ADS111X_ADDR_GND;
    // فول‌اسکیل ±4.096V و نرخ 128 SPS
    ads111x_gain_t gain = ADS111X_GAIN_4V096;
    ads111x_data_rate_t dr = ADS111X_DATA_RATE_860;

    /////////////////////////////////////////////////////////////////////////////////////////////
    // bool ads_ok = false;
    // esp_err_t err = ads1115_init(&adc, I2C_PORT, I2C_SDA_GPIO, I2C_SCL_GPIO, addr, gain, dr);
    // if (err == ESP_OK) {
    //     ads_ok = true;
    //     ESP_LOGI(TAG, "ADS1115 ready");
    // } else {
    //     ESP_LOGW(TAG, "ADS1115 not connected (OK for now, just testing build). err=%s",
    //             esp_err_to_name(err));
    // }
    /////////////////////////////////////////////////////////////////////////////////////////////
    esp_err_t err = ads1115_init(&adc, I2C_PORT, I2C_SDA_GPIO, I2C_SCL_GPIO, addr, gain, dr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADS1115 init failed! err=%s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "ADS1115 initialized OK");
    while (1) {
        if (xSemaphoreTake(timer_semaphore, portMAX_DELAY) == pdTRUE) {
            int idx_1hz  = (int)((angle_1hz  / (2.0 * PI)) * TABLE_SIZE) % TABLE_SIZE;
            int idx_50hz = (int)((angle_50hz / (2.0 * PI)) * TABLE_SIZE) % TABLE_SIZE;

            // استفاده از PHASE_SHIFT_30DEG برای ایجاد اختلاف فاز 30 درجه
            int idx_1hz_shifted  = (int)(((angle_1hz + PHASE_SHIFT_30DEG) / (2.0 * PI)) * TABLE_SIZE) % TABLE_SIZE;
            int idx_50hz_shifted = (int)(((angle_50hz + PHASE_SHIFT_30DEG) / (2.0 * PI)) * TABLE_SIZE) % TABLE_SIZE;

            float sample_1hz  = sine_table[idx_1hz];
            float sample_50hz = sine_table[idx_50hz];
            // تولید موج با اختلاف فاز 30 درجه برای 1Hz و 50Hz
            float sample_1hz_shifted  = sine_table[idx_1hz_shifted];
            float sample_50hz_shifted = sine_table[idx_50hz_shifted];

            // ایجاد موج ترکیبی با اختلاف فاز  
            float combined_signal_1hz_50hz = ((0.01 * sample_1hz) + sample_50hz);
            float combined_signal_1hz_50hz_shifted = 0.9*((-1)*(0.01 * sample_1hz_shifted) + sample_50hz_shifted);
            //////////////////////////////////////////////////////////////////////////////////////////

            ////////////////////////////////////////////////////////////////////////////////////////////
            ////////////////////////////////////////////////////////////////////////////////////////////
            
            float diff = combined_signal_1hz_50hz_shifted - combined_signal_1hz_50hz;
            float common = 0.5*(combined_signal_1hz_50hz_shifted + combined_signal_1hz_50hz);


            float phase_diff_common_ref = calculate_phase(diff, common);
            set_phase_shift_degrees(phase_diff_common_ref);
            float common_shift = process_sample(common);

            ////////////////////////////////////////////////
            // فیلتر کردن سیگنال 50 هرتز
            float filtered_50_diff = filter50Hz(&bp, diff);
            // محاسبه قله 50 هرتز
            float max_diff = updatePeak(&signalDiffTracker, filtered_50_diff);
            float max_common = updatePeak(&signalCommonTracker, common_shift);
            // ادامه پردازش، مثلا محاسبه phase
            float common_ref = common_shift * (max_diff / (max_common + 1e-12));
            ////////////////////////////////////////////////
            float out = common_ref + diff;

            // ESP_LOGI(TAG, "sig  = %.3f | shifted_sig = %.3f", diff, filtered_50_diff);
            ESP_LOGI(TAG, "sig  = %.3f | shifted_sig = %.3f", out, diff);


            angle_1hz  += PHASE_STEP_1HZ;
            angle_50hz += PHASE_STEP_50HZ;

            if (angle_1hz >= 2.0 * PI) angle_1hz -= 2.0 * PI;
            if (angle_50hz >= 2.0 * PI) angle_50hz -= 2.0 * PI;
            vTaskDelay(1);
        }
    }
}
