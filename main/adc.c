#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

const static char *TAGG = "ADC";

// ADC1 Channels
#if CONFIG_IDF_TARGET_ESP32
#define ADC1_CHAN0          ADC_CHANNEL_4
#define ADC1_CHAN1          ADC_CHANNEL_5
#define ADC1_CHAN2          ADC_CHANNEL_6
#else
#define ADC1_CHAN0          ADC_CHANNEL_2
#define ADC1_CHAN1          ADC_CHANNEL_3
#define ADC1_CHAN2          ADC_CHANNEL_4
#endif

#define ADC_ATTEN           ADC_ATTEN_DB_12
#define NUM_SAMPLES         1

static int adc_raw[3][NUM_SAMPLES];
static int voltage[3][NUM_SAMPLES];
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_chan0_handle = NULL;
static adc_cali_handle_t adc1_cali_chan1_handle = NULL;
static adc_cali_handle_t adc1_cali_chan2_handle = NULL;
static bool do_calibration1_chan0 = false;
static bool do_calibration1_chan1 = false;
static bool do_calibration1_chan2 = false;

typedef struct {
    int voltage_0;
    int voltage_1;
    int voltage_2;
} adc_values_t;

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);

void adc_init(void);
adc_values_t adc_read_data(void);
void adc_cleanup(void);

void adc_init(void) {
    adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN0, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN1, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN2, &config));

    do_calibration1_chan0 = adc_calibration_init(ADC_UNIT_1, ADC1_CHAN0, ADC_ATTEN, &adc1_cali_chan0_handle);
    do_calibration1_chan1 = adc_calibration_init(ADC_UNIT_1, ADC1_CHAN1, ADC_ATTEN, &adc1_cali_chan1_handle);
    do_calibration1_chan2 = adc_calibration_init(ADC_UNIT_1, ADC1_CHAN2, ADC_ATTEN, &adc1_cali_chan2_handle);
}

adc_values_t adc_read_data() {
    adc_values_t values = {0, 0, 0};

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN0, &adc_raw[0][0]));
    if (do_calibration1_chan0) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw[0][0], &voltage[0][0]));
        values.voltage_0 = voltage[0][0];
    }

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN1, &adc_raw[1][0]));
    if (do_calibration1_chan1) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan1_handle, adc_raw[1][0], &voltage[1][0]));
        values.voltage_1 = voltage[1][0];
    }

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN2, &adc_raw[2][0]));
    if (do_calibration1_chan2) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan2_handle, adc_raw[2][0], &voltage[2][0]));
        values.voltage_2 = voltage[2][0];
    }

    return values;
}

void adc_cleanup(void) {
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (adc1_cali_chan0_handle) {
        adc_calibration_deinit(adc1_cali_chan0_handle);
    }
    if (adc1_cali_chan1_handle) {
        adc_calibration_deinit(adc1_cali_chan1_handle);
    }
    if (adc1_cali_chan2_handle) {
        adc_calibration_deinit(adc1_cali_chan2_handle);
    }
}

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAGG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAGG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAGG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAGG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAGG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}
