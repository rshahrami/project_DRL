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
    SemaphoreHandle_t drdy_sem; // داده حاضر است
} ads131_t;

typedef enum {
    ADS131_RATE_976SPS,   // OSR=1024 (Default for 2MHz CLKIN)
    ADS131_RATE_3906SPS,  // OSR=256 (Approx 4kSPS)
    ADS131_RATE_7812SPS   // OSR=128 (Approx 8kSPS)
    // ... اضافه کردن سایر نرخ ها ...
} ads131_data_rate_t;

// init: host = SPI2_HOST or SPI3_HOST, pins as required, spi_freq_hz typical 2000000..8000000
esp_err_t ads131_init(ads131_t *dev, spi_host_device_t host,
                      int miso_io, int mosi_io, int sclk_io,
                      gpio_num_t cs_gpio, gpio_num_t drdy_gpio, gpio_num_t reset_gpio,
                      int spi_freq_hz);


esp_err_t ads131_set_data_rate(ads131_t *dev, ads131_data_rate_t rate);

// basic register read/write (device-specific widths)
esp_err_t ads131_read_register(ads131_t *dev, uint8_t address, uint16_t *value);
esp_err_t ads131_write_register(ads131_t *dev, uint8_t address, uint16_t value);

// send command (16-bit command)
esp_err_t ads131_command(ads131_t *dev, uint16_t cmd);

// read one data frame (raw 24-bit × channels). channels_count depends on part (e.g., 4 or 8)
esp_err_t ads131_read_frame_raw(ads131_t *dev, int32_t *out_samples, size_t channels);

// helper: blocking wait for DRDY (timeout_ms) - uses drdy_sem set by ISR
bool ads131_wait_drdy(ads131_t *dev, TickType_t ticks_to_wait);

// deinit
void ads131_deinit(ads131_t *dev);
