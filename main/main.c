#include "timer.c"
#include "adc.c"
#include "wave.c"
#include "i2c.c"
#include "max_value.c"
#include "phase_detect.c"
#include "phase_shifter.c"


// #include "dac.c"
// #include "driver/dac_cosine.h"
const static char *TAG = "main";
// const static char *TAG_0 = ""; // اگر استفاده نمی‌شود، می‌توانید حذف کنید
const static char *TAG_1 = "SIN_LOG"; // برای لاگ کردن موج‌ها

volatile int ads_state = 0;
volatile int16_t adc_val_ain0 = 0;
volatile int16_t adc_val_ain1 = 0;

#define PI 3.14159265358979323846
#define APP_SAMPLE_RATE 2000.0  // نرخ نمونه برداری اصلی سیستم
#define SINE_TABLE_SIZE 2000    // اندازه جدول سینوسی

// فاصله زمانی تایمر برای تمام وظایفی که با نرخ نمونه‌برداری اجرا می‌شوند
const double TIMER_INTERVAL_S = 1.0 / APP_SAMPLE_RATE;

// متغیرهای انباشت فاز برای هر موج
double current_angle_1hz = 0.0;
double current_angle_50hz = 0.0;

// گام افزایش فاز در هر نمونه برای هر موج
// برای موج 1 هرتز: (2 * PI * فرکانس_موج) / نرخ_نمونه_برداری
const double PHASE_STEP_1HZ = (2.0 * PI * 1.0) / APP_SAMPLE_RATE;
// برای موج 50 هرتز:
const double PHASE_STEP_50HZ = (2.0 * PI * 50.0) / APP_SAMPLE_RATE;



void app_main(void) {
    // uint16_t result = 0; // برای ADC
    float sig_1 = 0.0;
    float sig_2 = 0.0;
    // float elec_1 = 0.0;
    // float elec_2 = 0.0;

    timer_semaphore = xSemaphoreCreateBinary(); // مطمئن شوید timer_semaphore به درستی تعریف شده (مثلا SemaphoreHandle_t)

    ESP_ERROR_CHECK(esp_task_wdt_deinit());

    gptimer_handle_t timer_handle_adc;
    // تایمر ADC با نرخ نمونه‌برداری اصلی اجرا می‌شود
    timer_init(TIMER_INTERVAL_S, &timer_handle_adc, 0); // timer_id = 0 for ADC

    gptimer_handle_t timer_handle_1hz;
    // تایمر موج 1 هرتز نیز با نرخ نمونه‌برداری اصلی اجرا می‌شود
    timer_init(TIMER_INTERVAL_S, &timer_handle_1hz, 1); // timer_id = 1 for 1Hz sine

    gptimer_handle_t timer_handle_50hz;
    // تایمر موج 50 هرتز نیز با نرخ نمونه‌برداری اصلی اجرا می‌شود
    timer_init(TIMER_INTERVAL_S, &timer_handle_50hz, 2); // timer_id = 2 for 50Hz sine

    // Setup I2C
    i2c_param_config(I2C_NUM, &i2c_cfg);
    i2c_driver_install(I2C_NUM, I2C_MODE, I2C_RX_BUF_STATE, I2C_TX_BUF_STATE, I2C_INTR_ALOC_FLAG);

    // Setup ADS1115
    ADS1115_initiate(&ads1115_cfg);
    double epsilon = 0.000000001;

        // ADS1115_request_single_ended_AIN1()
    adc_init(); // مقداردهی اولیه ADC داخلی ESP32 (اگر از آن استفاده می‌کنید)

    create_sine_lookup_table(); // ساخت جدول سینوسی
    // int counter = 0; // برای تست ADC
    while (1) {
        if (xSemaphoreTake(timer_semaphore, portMAX_DELAY) == pdTRUE) {
            switch (current_timer_id) {
                case 0:
                    switch (ads_state) {
                        case 0:  // مرحله درخواست AIN0
                            ADS1115_request_single_ended_AIN0();
                            ads_state = 1;
                            break;
                
                        case 1:  // چک کن آیا تبدیل AIN0 تمام شده
                            if (!ADS1115_get_conversion_state()) {
                                adc_val_ain0 = ADS1115_get_conversion();
                                ADS1115_request_single_ended_AIN1();
                                ads_state = 2;
                            }
                            break;
                
                        case 2:  // چک کن آیا تبدیل AIN1 تمام شده
                            if (!ADS1115_get_conversion_state()) {
                                adc_val_ain1 = ADS1115_get_conversion();
                                // اینجا می‌تونی پردازش یا ذخیره انجام بدی
                                ads_state = 0;
                            }
                            break;
                    }
                    break;



                    //////////////////////////////////////////////////////////////////////////////////
                    // float sig_diff = sig_1 - sig_2;
                    // float sig_common = sig_2;
                    // float max_diff = max_signal_diff(sig_diff);
                    // float max_comman = max_signal_common(sig_common);
                    // float ref = sig_common * (max_diff/(max_comman + epsilon));
                    // float phase_diff = calculate_phase_diff(ref, sig_diff);
                    // set_phase_shift_degrees(phase_diff);
                    // float signal_common_shift = process_sample(ref);
                    // float sig_out = signal_common_shift + sig_diff;
                    //////////////////////////////////////////////////////////////////////////////////
                    // ESP_LOGI(TAG_1, "50Hz: %f", sig_out);

                // ############################################################
                case 1: // تولید نمونه برای موج 1 هرتز
                    // محاسبه اندیس صحیح برای جدول سینوسی
                    sig_1 = 0.01 * (sine_lookup_table[(int)((current_angle_1hz / (2.0 * PI)) * SINE_TABLE_SIZE) % SINE_TABLE_SIZE]);
                    
                    current_angle_1hz += PHASE_STEP_1HZ; // افزایش فاز
                    // بازگرداندن فاز به محدوده 0 تا 2*PI
                    if (current_angle_1hz >= 2.0 * PI) {
                        current_angle_1hz -= 2.0 * PI;
                    }
                    // ESP_LOGI(TAG_1, "1Hz: %f", sig_1); // لاگ کردن مقدار موج 1 هرتز
                    break;
                case 2: // تولید نمونه برای موج 50 هرتز
                    // محاسبه اندیس صحیح برای جدول سینوسی
                    sig_2 = sine_lookup_table[(int)((current_angle_50hz / (2.0 * PI)) * SINE_TABLE_SIZE) % SINE_TABLE_SIZE];
                    
                    current_angle_50hz += PHASE_STEP_50HZ; // افزایش فاز
                    // بازگرداندن فاز به محدوده 0 تا 2*PI
                    if (current_angle_50hz >= 2.0 * PI) {
                        current_angle_50hz -= 2.0 * PI;
                    }
                    // برای مشاهده موج 50 هرتز، آن را نیز لاگ کنید
                    // ESP_LOGI(TAG_1, "50Hz: %f", sig_2);
                    break;
                // ############################################################
                default:
                    ESP_LOGE(TAG, "Unknown Timer Event");
            }

        }
    }

    adc_cleanup();
}
