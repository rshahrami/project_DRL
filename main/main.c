#include "timer.c"
#include "adc.c"
#include "wave.c"
#include "i2c.c"

// #include "dac.c"
// #include "driver/dac_cosine.h"
const static char *TAG = "main";
// const static char *TAG_0 = "";
const static char *TAG_1 = "";

#define PI 3.14159265
#define sample_rate 100.0

float R = 1000;
float C = 0.00001;

float prevInput = 0.0;
float prevOutput = 0.0;


#define PI 3.14159265
#define sample_rate 100.0
#define TABLE_SIZE 100

double TIMER_INTERVAL_SEC_ADC = 1;
float sum = 0;
float diff = 0;
float A = 1;
float Vref = 0;

// ############################################################
// # sin func settings
double TIMER_INTERVAL_SEC_1HZ = 1.0 / sample_rate;            // 100 samples per cycle for 1 Hz 
double TIMER_INTERVAL_SEC_50HZ = 1.0 / (sample_rate * 50.0);  // 100 samples per cycle for 50 Hz

double angle_1hz = 0; 
double angle_50hz = 0; 
double step_1hz = 2 * PI / sample_rate;         // 100 samples per cycle 
double step_50hz = 2 * PI / (sample_rate * 50); // 100 samples per cycle for 50 Hz
// ############################################################


double sine_table[TABLE_SIZE];

void createSineTable() {
    for (int i = 0; i < TABLE_SIZE; i++) {
        sine_table[i] = A * sin(2 * PI * i / TABLE_SIZE);
    }
}


float integrator(float input){
    float output = prevOutput + ((input + prevInput)/(2*R*C));
    return output;
}

volatile int sine_index_1hz = 0;
volatile int sine_index_50hz = 0;

void app_main(void) {
    uint16_t result = 0;
    // double result = 0;
    float sig_1 = 0.0;
    float sig_2 = 0.0;
    float elec_1 = 0.0;
    float elec_2 = 0.0;

    timer_semaphore = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(esp_task_wdt_deinit());

    gptimer_handle_t timer_handle_adc;
    timer_init(TIMER_INTERVAL_SEC_ADC, &timer_handle_adc, 0);

    gptimer_handle_t timer_handle_1hz;
    timer_init(TIMER_INTERVAL_SEC_1HZ, &timer_handle_1hz, 1);

    gptimer_handle_t timer_handle_50hz;
    timer_init(TIMER_INTERVAL_SEC_50HZ, &timer_handle_50hz, 2);

    // Setup I2C
    i2c_param_config(I2C_NUM, &i2c_cfg);
    i2c_driver_install(I2C_NUM, I2C_MODE, I2C_RX_BUF_STATE, I2C_TX_BUF_STATE, I2C_INTR_ALOC_FLAG);

    // Setup ADS1115
    ADS1115_initiate(&ads1115_cfg);

    // ADS1115_request_single_ended_AIN1();
    
    
    // ############################################################
    // gptimer_handle_t timer_handle_1hz;
    // timer_init(TIMER_INTERVAL_SEC_1HZ, &timer_handle_1hz, 1);

    // gptimer_handle_t timer_handle_50hz;
    // timer_init(TIMER_INTERVAL_SEC_50HZ, &timer_handle_50hz, 2);
    // ############################################################

    adc_init();
    // dac_init();
    createSineTable();

    while (1) {
        if (xSemaphoreTake(timer_semaphore, portMAX_DELAY) == pdTRUE) {
            switch (current_timer_id) {
                case 0:
                    // adc_values_t values = adc_read_data();
                    ADS1115_request_single_ended_AIN1();
                    // values.voltage_0;
                    // ESP_LOGI(TAG, "Voltage_0: %d mV, Voltage_1: %d mV, Voltage_2: %d mV", values.voltage_0, values.voltage_1, values.voltage_2);
                    // ESP_LOGI(TAG_0, " %d\n", values.voltage_0);
                    result = ADS1115_get_conversion();
                    // sum = (values.voltage_0 + values.voltage_1)/2;
                    // diff = (A * (values.voltage_0 - values.voltage_1)) + Vref;
                    ESP_LOGI(TAG_1, " %d\n", result);  
                    break;
                // ############################################################
                case 1:
                    sig_1 = sine_table[(int)(angle_1hz / step_1hz) % TABLE_SIZE];
                    ESP_LOGI(TAG, "Sine Wave 1Hz: %f", sine_table[(int)(angle_1hz / step_1hz) % TABLE_SIZE]);
                    angle_1hz += step_1hz;
                    if (angle_1hz >= 2 * PI) {
                        angle_1hz -= 2 * PI;
                    }
                   
                    break;
                case 2:
                    sig_2 = sine_table[(int)(angle_50hz / step_50hz) % TABLE_SIZE];
                    ESP_LOGI(TAG, "Sine Wave 50Hz: %f", sine_table[(int)(angle_50hz / step_50hz) % TABLE_SIZE]);
                    angle_50hz += step_50hz;
                    if (angle_50hz >= 2 * PI) {
                        angle_50hz -= 2 * PI;
                    }
                    break;
                // ############################################################
                default:
                    ESP_LOGE(TAG, "Unknown Timer Event");
            }
        }
    }

    adc_cleanup();
}
