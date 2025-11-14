// phase_detect.c
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef SAMPLE_RATE_HZ
#define SAMPLE_RATE_HZ 1000.0f     // اگر جایی تعریف شده، همین را می‌گیرد
#endif
#ifndef SIGNAL_FREQ
#define SIGNAL_FREQ 50.0f
#endif

// فیلتر پایین‌گذر تک‌قطبی برای هموارسازی I/Q و خود اختلاف فاز
static float beta;        // 0<beta<1  ~ پهنای باند LPF
static float omega;       // 2π f0 / Fs
static float phase_acc;   // فاز مرجع 50Hz هم‌فاز با نمونه‌برداری
static int   inited;

static float i1,q1,i2,q2; // مولفه‌های درجا
static float d_lp;        // اختلاف فاز هموارشده

static inline float lpf(float y, float x) { return y + beta * (x - y); }

static void init_lockin(float fc_lpf_hz) {
    omega = 2.0f * (float)M_PI * (float)SIGNAL_FREQ / (float)SAMPLE_RATE_HZ;
    beta  = 1.0f - expf(-2.0f * (float)M_PI * fc_lpf_hz / (float)SAMPLE_RATE_HZ); // تک‌قطبی
    phase_acc = 0.0f;
    i1=q1=i2=q2=d_lp=0.0f;
    inited = 1;
}

// خروجی: فاز s1 نسبت به s2 بر حسب درجه در بازه [-180,+180]
float calculate_phase(float s1, float s2) {
    if (!inited) init_lockin(4.0f); // پهنای باند LPF ≈ 4Hz (۱–۲ پریود قفل)

    // مرجع سینوس/کسینوس 50Hz
    float c = cosf(phase_acc);
    float s = sinf(phase_acc);
    phase_acc += omega;
    if (phase_acc >= 2.0f*(float)M_PI) phase_acc -= 2.0f*(float)M_PI;

    // دمدولاسیون I/Q و پایین‌گذر
    float xi1 = s1 * c, xq1 = s1 * s;
    float xi2 = s2 * c, xq2 = s2 * s;

    i1 = lpf(i1, xi1);  q1 = lpf(q1, xq1);
    i2 = lpf(i2, xi2);  q2 = lpf(q2, xq2);

    // فاز لحظه‌ای هر سیگنال
    float ph1 = atan2f(q1, i1);
    float ph2 = atan2f(q2, i2);

    // اختلاف فاز و نرمال‌سازی به [-π, π]
    float d = ph1 - ph2;
    if (d >  (float)M_PI) d -= 2.0f*(float)M_PI;
    if (d < -(float)M_PI) d += 2.0f*(float)M_PI;

    // هموارسازی خروجی برای حذف لرزش
    d_lp = lpf(d_lp, d);

    return d_lp * (180.0f/(float)M_PI);
}

///////////////////////////////////////////////////////////////////////////////////
// #include <stdint.h>
// #include <stdio.h>

// #define SIGNAL_FREQ       50
// #define SAMPLE_RATE       1000.0f
// #define PERIOD_SAMPLES    ((float)(SAMPLE_RATE / SIGNAL_FREQ))
// #define SAMPLES_PER_DEGREE (PERIOD_SAMPLES / 360.0f)

// float calculate_phase(float s1, float s2) {
//     static float prev_s1 = 0.0f;
//     static float prev_s2 = 0.0f;
//     static float last_zc1 = -1.0f;
//     static float last_zc2 = -1.0f;
//     static uint32_t sample_counter = 0;

//     sample_counter++;

//     // عبور از صفر با خطی‌سازی
//     if (prev_s1 <= 0.0f && s1 > 0.0f) {
//         last_zc1 = (float)(sample_counter - 1) + (0.0f - prev_s1) / (s1 - prev_s1);
//     }

//     if (prev_s2 <= 0.0f && s2 > 0.0f) {
//         last_zc2 = (float)(sample_counter - 1) + (0.0f - prev_s2) / (s2 - prev_s2);
//     }

//     prev_s1 = s1;
//     prev_s2 = s2;

//     if (last_zc1 >= 0 && last_zc2 >= 0) {
//         float delta_samples = last_zc1 - last_zc2;

//         // نرمال کردن اختلاف نمونه‌ها به بازه [-PERIOD_SAMPLES/2, +PERIOD_SAMPLES/2]
//         while (delta_samples > PERIOD_SAMPLES / 2.0f) delta_samples -= PERIOD_SAMPLES;
//         while (delta_samples < -PERIOD_SAMPLES / 2.0f) delta_samples += PERIOD_SAMPLES;

//         // تبدیل به درجه
//         float phase_deg = delta_samples / SAMPLES_PER_DEGREE;

//         return phase_deg;
//     }

//     return 0.0f;
// }

