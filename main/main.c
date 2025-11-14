#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "phase_detect.h"
#include "phase_shifter.h"

// مقداردهی قبل از include هدرها
#define SIGNAL_FREQ       60.0f
#define SAMPLE_RATE_HZ    2000.0f

static const char *TAG = "main";


void app_main(void)
{
    ESP_LOGI(TAG, "ADS1115 ready: range=±6.144V, DR=860SPS, I2C=400kHz (NG)");

    // شروع تسک خواندن
}
