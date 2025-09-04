// timer.c
#include "timer.h"
#include "esp_log.h"

static const char *TAG = "timer";

SemaphoreHandle_t timer_semaphore = NULL;
volatile int current_timer_id = -1;

bool IRAM_ATTR timer_isr_cb(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx
) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(timer_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    return true;
}

void timer_init_hz(double freq_hz, gptimer_handle_t *timer_handle, int timer_id) {
    if (timer_semaphore == NULL) {
        timer_semaphore = xSemaphoreCreateBinary();
        configASSERT(timer_semaphore);
    }

    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&cfg, timer_handle));

    uint64_t alarm_count = (uint64_t)(cfg.resolution_hz / freq_hz);
    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = alarm_count,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(*timer_handle, &alarm_cfg));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_isr_cb
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(*timer_handle, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(*timer_handle));
    ESP_ERROR_CHECK(gptimer_start(*timer_handle));

    ESP_LOGI(TAG, "Timer started at %.2f Hz", freq_hz);
}
