// phase_detect.h
#ifndef PHASE_DETECT_H
#define PHASE_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ================== توضیح ==================

  calculate_phase_rt():

  محاسبه اختلاف فاز بین دو سیگنال 50Hz به صورت real-time
  - فقط مؤلفه 50Hz در نظر گرفته می‌شود
  - خروجی پایدار با latency کم
  - مناسب برای کنترل و مانیتورینگ لحظه‌ای

  ورودی:
    s1 , s2 : نمونه‌های لحظه‌ای (مثلاً ولت)

  خروجی:
    اختلاف فاز s1 نسبت به s2 بر حسب درجه
    بازه: [-180 , +180]

=========================================== */

float calculate_phase_rt(float s1, float s2);

#ifdef __cplusplus
}
#endif

#endif // PHASE_DETECT_H
