#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"

#define RESOLUTION  1 * 1000 * 1000

SemaphoreHandle_t timer_semaphore;
static volatile int current_timer_id = -1;

static bool IRAM_ATTR timer_isr_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    current_timer_id = (int) user_ctx;
    xSemaphoreGiveFromISR(timer_semaphore, NULL);
    return true;
}

void timer_init(double interupt, gptimer_handle_t *timer_handle, int timer_id) {
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = RESOLUTION,
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, timer_handle));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = interupt * RESOLUTION,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(*timer_handle, &alarm_config));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_isr_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(*timer_handle, &cbs, (void *)timer_id));

    ESP_ERROR_CHECK(gptimer_enable(*timer_handle));
    ESP_ERROR_CHECK(gptimer_start(*timer_handle));
}
