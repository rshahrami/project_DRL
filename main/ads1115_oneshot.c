// main/ads1115_oneshot.c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"

#include "i2cdev.h"
#include "ads111x.h"
#include "ads1115_oneshot.h"

static const char *TAG = "ads1115_mod";

/* تبدیل گِین به فول‌اسکیل ولتاژ (ولت) */
static inline float fsr_volts(ads111x_gain_t g)
{
    switch (g) {
        case ADS111X_GAIN_6V144:  return 6.144f;
        case ADS111X_GAIN_4V096:  return 4.096f;
        case ADS111X_GAIN_2V048:  return 2.048f;
        case ADS111X_GAIN_1V024:  return 1.024f;
        case ADS111X_GAIN_0V512:  return 0.512f;
        case ADS111X_GAIN_0V256:
        case ADS111X_GAIN_0V256_2:
        case ADS111X_GAIN_0V256_3: return 0.256f;
        default: return 4.096f;
    }
}

esp_err_t ads1115_init(ads1115_ctx_t *ctx,
                       i2c_port_t port,
                       gpio_num_t sda,
                       gpio_num_t scl,
                       uint8_t addr,
                       ads111x_gain_t gain,
                       ads111x_data_rate_t dr)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    /* لایه‌ی مشترک i2cdev (ایمن برای چندبار فراخوانی) */
    ESP_RETURN_ON_ERROR(i2cdev_init(), TAG, "i2cdev_init");

    /* قبل از ساخت دیسکریپتور، همه‌ی فیلدهای موردنیاز را ست کن تا init_desc همان را به‌کار بگیرد */
    ctx->dev.port = port;
    ctx->dev.addr = addr;
    ctx->dev.cfg.sda_io_num = sda;
    ctx->dev.cfg.scl_io_num = scl;
    ctx->dev.cfg.master.clk_speed = 400000;   // سرعت باس I²C روی 400kHz

    /* ساخت دیسکریپتور ADS1115 با تنظیمات بالا (I2C NG) */
    ESP_RETURN_ON_ERROR(ads111x_init_desc(&ctx->dev, addr, port, sda, scl), TAG, "init_desc");
    ESP_LOGI(TAG, "I2C clock set to 400 kHz via i2cdev (NG)");

    /* پارامترهای تبدیل را ذخیره و روی تراشه اعمال کن */
    ctx->gain = gain;
    ctx->dr   = dr;

    ESP_RETURN_ON_ERROR(ads111x_set_gain(&ctx->dev, ctx->gain), TAG, "set_gain");
    ESP_RETURN_ON_ERROR(ads111x_set_data_rate(&ctx->dev, ctx->dr), TAG, "set_dr");
    ESP_RETURN_ON_ERROR(ads111x_set_mode(&ctx->dev, ADS111X_MODE_SINGLE_SHOT), TAG, "set_mode");

    return ESP_OK;
}

/* انتخاب مالتی‌پلکسر بر اساس شماره کانال single-ended */
static esp_err_t mux_from_channel(int ch, ads111x_mux_t *m)
{
    if (!m) return ESP_ERR_INVALID_ARG;
    switch (ch) {
        case 0: *m = ADS111X_MUX_0_GND; break;
        case 1: *m = ADS111X_MUX_1_GND; break;
        case 2: *m = ADS111X_MUX_2_GND; break;
        case 3: *m = ADS111X_MUX_3_GND; break;
        default: return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t ads1115_read_single_ended(ads1115_ctx_t *ctx, int channel, int16_t *raw, float *volts)
{
    if (!ctx || !raw) return ESP_ERR_INVALID_ARG;

    ads111x_mux_t mux;
    ESP_RETURN_ON_ERROR(mux_from_channel(channel, &mux), TAG, "bad_channel");

    /* انتخاب ورودی و پارامترها برای این تک‌شات */
    ESP_RETURN_ON_ERROR(ads111x_set_input_mux(&ctx->dev, mux), TAG, "set_mux");
    ESP_RETURN_ON_ERROR(ads111x_set_gain(&ctx->dev, ctx->gain), TAG, "set_gain");
    ESP_RETURN_ON_ERROR(ads111x_set_data_rate(&ctx->dev, ctx->dr), TAG, "set_dr");
    ESP_RETURN_ON_ERROR(ads111x_set_mode(&ctx->dev, ADS111X_MODE_SINGLE_SHOT), TAG, "set_mode");

    /* شروع تبدیل (OS=1) */
    ESP_RETURN_ON_ERROR(ads111x_start_conversion(&ctx->dev), TAG, "start");

    /* پولینگ تا پایان تبدیل */
    bool busy = true;
    while (busy) {
        ESP_RETURN_ON_ERROR(ads111x_is_busy(&ctx->dev, &busy), TAG, "busy");
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    /* خواندن مقدار خام */
    ESP_RETURN_ON_ERROR(ads111x_get_value(&ctx->dev, raw), TAG, "get_value");

    /* تبدیل به ولتاژ (ولت) */
    if (volts) {
        float fsr = fsr_volts(ctx->gain);            // ±FSR
        *volts = ((float)(*raw) / 32768.0f) * fsr;   // اسکیل به ولت
        if (*volts < 0) *volts = 0;                  // برای single-ended نویز منفی کوچک را صفر کن
    }

    return ESP_OK;
}

void ads1115_deinit(ads1115_ctx_t *ctx)
{
    if (!ctx) return;
    ads111x_free_desc(&ctx->dev);
}
