// timer.h

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gptimer.h"

// سمفور برای همگام‌سازی با ISR
extern SemaphoreHandle_t timer_semaphore;

// تابع راه‌اندازی تایمر با فرکانس مشخص (در Hz)
void timer_init_hz(double freq_hz, gptimer_handle_t *timer_handle, int timer_id);
