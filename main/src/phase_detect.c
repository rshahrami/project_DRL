// phase_detect.c
#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================== تنظیمات ================== */

#ifndef SAMPLE_RATE_HZ
#define SAMPLE_RATE_HZ 7812.0f   // نرخ واقعی نمونه‌برداری تو
#endif

#ifndef SIGNAL_FREQ_HZ
#define SIGNAL_FREQ_HZ 50.0f
#endif

// پهنای باند فیلتر روی فاز (Hz)
// هرچه کوچکتر → jitter کمتر، latency بیشتر
#define PHASE_LPF_HZ   3.0f

// حذف DC (خیلی آهسته، بدون اثر روی فاز)
#define DC_ALPHA       0.001f

/* ================== متغیرهای داخلی ================== */

static bool  inited = false;

// مرجع 50Hz هم‌فاز با نمونه‌برداری
static float omega;
static float phase_acc;

// مولفه‌های I/Q
static float i1, q1, i2, q2;

// فاز هموارشده خروجی
static float phase_lp;

// DC trackers
static float dc1, dc2;

// ضریب LPF تک‌قطبی
static float beta;

/* ================== ابزارها ================== */

static inline float lpf(float y, float x)
{
    return y + beta * (x - y);
}

static inline float remove_dc(float x, float *dc)
{
    *dc += DC_ALPHA * (x - *dc);
    return x - *dc;
}

/* ================== راه‌اندازی ================== */

static void phase_init(void)
{
    omega = 2.0f * (float)M_PI * SIGNAL_FREQ_HZ / SAMPLE_RATE_HZ;

    // LPF تک‌قطبی: beta = 1 - exp(-2π fc / Fs)
    beta = 1.0f - expf(-2.0f * (float)M_PI * PHASE_LPF_HZ / SAMPLE_RATE_HZ);

    phase_acc = 0.0f;

    i1 = q1 = i2 = q2 = 0.0f;
    dc1 = dc2 = 0.0f;
    phase_lp = 0.0f;

    inited = true;
}

/* ================== محاسبه اختلاف فاز real-time ================== */
/*
 * ورودی:
 *   s1 , s2  : نمونه‌های لحظه‌ای دو سیگنال (ولت)
 *
 * خروجی:
 *   اختلاف فاز s1 نسبت به s2 بر حسب درجه در بازه [-180 , +180]
 *
 * ویژگی‌ها:
 *   - فقط مؤلفه 50Hz
 *   - real-time
 *   - latency ≈ کسری از سیکل
 */
float calculate_phase_rt(float s1, float s2)
{
    if (!inited)
        phase_init();

    /* ---------- حذف DC ---------- */
    float x1 = remove_dc(s1, &dc1);
    float x2 = remove_dc(s2, &dc2);

    /* ---------- مرجع سین/کسین ---------- */
    float c = cosf(phase_acc);
    float s = sinf(phase_acc);

    phase_acc += omega;
    if (phase_acc >= 2.0f * (float)M_PI)
        phase_acc -= 2.0f * (float)M_PI;

    /* ---------- Lock-in (I/Q) ---------- */
    i1 = lpf(i1, x1 * c);
    q1 = lpf(q1, x1 * s);

    i2 = lpf(i2, x2 * c);
    q2 = lpf(q2, x2 * s);

    /* ---------- فاز لحظه‌ای ---------- */
    float ph1 = atan2f(q1, i1);
    float ph2 = atan2f(q2, i2);

    float d = ph1 - ph2;

    /* ---------- نرمال‌سازی ---------- */
    if (d >  (float)M_PI) d -= 2.0f * (float)M_PI;
    if (d < -(float)M_PI) d += 2.0f * (float)M_PI;

    /* ---------- هموارسازی بسیار ملایم ---------- */
    phase_lp = lpf(phase_lp, d);

    return phase_lp * (180.0f / (float)M_PI);
}
