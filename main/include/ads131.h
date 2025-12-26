#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

typedef struct {
    spi_device_handle_t spi;
    gpio_num_t cs_gpio;
    gpio_num_t drdy_gpio;
    gpio_num_t reset_gpio;
    SemaphoreHandle_t drdy_sem;
} ads131_t;

typedef struct {
    int32_t status;
    int32_t ch[4];
} ads131_frame_t;

typedef enum {
    ADS131_RATE_976SPS,
    ADS131_RATE_3906SPS,
    ADS131_RATE_7812SPS,
} ads131_data_rate_t;

esp_err_t ads131_init(
    ads131_t *dev,
    spi_host_device_t host,
    int miso_io,
    int mosi_io,
    int sclk_io,
    gpio_num_t cs_gpio,
    gpio_num_t drdy_gpio,
    gpio_num_t reset_gpio,
    int spi_freq_hz
);

esp_err_t ads131_set_gain_1_all(ads131_t *dev);
esp_err_t ads131_set_data_rate(ads131_t *dev, ads131_data_rate_t rate);
esp_err_t ads131_command(ads131_t *dev, uint16_t cmd);
bool ads131_wait_drdy(ads131_t *dev, TickType_t timeout);
esp_err_t ads131_read_frame(ads131_t *dev, ads131_frame_t *frame);
void ads131_deinit(ads131_t *dev);
