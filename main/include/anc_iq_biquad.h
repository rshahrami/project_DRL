#ifndef ANC_IQ_BIQUAD_H
#define ANC_IQ_BIQUAD_H

#include <math.h>
#include <string.h>

/*
  ANC قوی‌تر:
  - دو Bandpass مستقل (state جدا) برای COM و DIFF
  - Adaptive FIR با NLMS (چندتپی) برای دنبال کردن فاز/تاخیر/لغزش
*/

#ifndef ANC_NTAPS
#define ANC_NTAPS   21
#endif

#ifndef ANC_EPS
#define ANC_EPS     1e-6f
#endif

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float z1, z2;
} biquad_bp_t;

typedef struct {
    /* NLMS FIR */
    float w[ANC_NTAPS];
    float x[ANC_NTAPS];
    float mu;

    /* دو bandpass مستقل */
    biquad_bp_t bp_ref;  // برای COM
    biquad_bp_t bp_d;    // برای DIFF

    /* limiter برای مرجع */
    float ref_lim;

    /* (اختیاری) لیک کوچک برای جلوگیری از ران‌اوت */
    float leak;
} anc_t;

/* ========== Bandpass (constant peak gain = 1) ========== */
static inline void biquad_bp_init(biquad_bp_t *bq, float fs, float f0, float Q)
{
    float w0 = 2.0f * (float)M_PI * f0 / fs;
    float sn = sinf(w0);
    float cs = cosf(w0);
    float alpha = sn / (2.0f * Q);

    /* constant peak gain */
    float b0 =  alpha;
    float b1 =  0.0f;
    float b2 = -alpha;
    float a0 =  1.0f + alpha;
    float a1 = -2.0f * cs;
    float a2 =  1.0f - alpha;

    bq->b0 = b0 / a0;
    bq->b1 = b1 / a0;
    bq->b2 = b2 / a0;
    bq->a1 = a1 / a0;
    bq->a2 = a2 / a0;

    bq->z1 = 0.0f;
    bq->z2 = 0.0f;
}

static inline float biquad_bp_process(biquad_bp_t *bq, float x)
{
    /* Direct Form II Transposed */
    float y = bq->b0 * x + bq->z1;
    bq->z1 = bq->b1 * x - bq->a1 * y + bq->z2;
    bq->z2 = bq->b2 * x - bq->a2 * y;
    return y;
}

/* ====== wrappers برای خوانایی ====== */
static inline void anc_bandpass_init(anc_t *a, float fs, float f0, float Q)
{
    biquad_bp_init(&a->bp_ref, fs, f0, Q);
    biquad_bp_init(&a->bp_d,   fs, f0, Q);
}

static inline float anc_ref_bp_process(anc_t *a, float x_ref)
{
    return biquad_bp_process(&a->bp_ref, x_ref);
}

static inline float anc_d_bp_process(anc_t *a, float x_d)
{
    return biquad_bp_process(&a->bp_d, x_d);
}

/* ========== FIR-NLMS ========== */
/* خروجی: y = نویز تخمینی (در باند 50 روی مسیر DIFF) */
static inline float anc_fir_nlms(anc_t *a, float ref, float d)
{
    /* shift reg */
    for (int i = ANC_NTAPS - 1; i > 0; --i) a->x[i] = a->x[i - 1];
    a->x[0] = ref;

    float y = 0.0f;
    float p = ANC_EPS;

    for (int i = 0; i < ANC_NTAPS; ++i) {
        y += a->w[i] * a->x[i];
        p += a->x[i] * a->x[i];
    }

    float e = d - y;         /* خطا */
    float g = a->mu / p;     /* NLMS gain */

    /* لیک کوچک برای جلوگیری از رشد آرام وزن‌ها */
    float leak = a->leak;
    if (leak > 0.0f) {
        for (int i = 0; i < ANC_NTAPS; ++i)
            a->w[i] *= (1.0f - leak);
    }

    for (int i = 0; i < ANC_NTAPS; ++i)
        a->w[i] += g * e * a->x[i];

    return y;
}

/* ========== init ========== */
static inline void anc_init(anc_t *a, float fs)
{
    memset(a, 0, sizeof(*a));

    /* این‌ها رو طوری گذاشتم که “زیاد کردن” رخ نده */
    a->mu      = 0.05f;     /* از 0.10 کمتر: پایدارتر */
    a->ref_lim = 1200.0f;
    a->leak    = 0.0005f;

    /* پیش‌فرض: 50Hz با Q متوسط/پهن */
    anc_bandpass_init(a, fs, 50.0f, 1.5f);
}

#endif
