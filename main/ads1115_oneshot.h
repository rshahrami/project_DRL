#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#include "i2cdev.h"   // مهم: نوع i2c_dev_t اینجاست
#include "ads111x.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_dev_t dev;              // قبلاً ads111x_t بود؛ در این نسخه i2c_dev_t درسته
    ads111x_gain_t gain;
    ads111x_data_rate_t dr;
} ads1115_ctx_t;

/**
 * @brief مقداردهی اولیّه ADS1115 برای حالت oneshot
 */
esp_err_t ads1115_init(ads1115_ctx_t *ctx,
                       i2c_port_t port,
                       gpio_num_t sda,
                       gpio_num_t scl,
                       uint8_t addr,                    // قبلاً ads111x_address_t
                       ads111x_gain_t gain,
                       ads111x_data_rate_t dr);

/**
 * @brief خواندن single-ended به صورت oneshot از کانال 0..3
 */
esp_err_t ads1115_read_single_ended(ads1115_ctx_t *ctx, int channel, int16_t *raw, float *volts);

/**
 * @brief آزادسازی دیسکریپتور
 */
void ads1115_deinit(ads1115_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
