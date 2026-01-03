#pragma once
#include <math.h>
#include <stdint.h>

typedef struct {
    float b0,b1,b2,a1,a2;
    float z1,z2;
} biquad_t;

static inline void biquad_init_bandpass(biquad_t *s, float fs, float f0, float Q)
{
    float w0 = 2.0f*(float)M_PI*f0/fs;
    float c = cosf(w0);
    float sn = sinf(w0);
    float alpha = sn/(2.0f*Q);

    // Bandpass (constant skirt gain, peak gain=Q)
    float b0 =  alpha;
    float b1 =  0.0f;
    float b2 = -alpha;
    float a0 =  1.0f + alpha;
    float a1 = -2.0f*c;
    float a2 =  1.0f - alpha;

    s->b0 = b0/a0; s->b1 = b1/a0; s->b2 = b2/a0;
    s->a1 = a1/a0; s->a2 = a2/a0;
    s->z1 = 0; s->z2 = 0;
}

static inline float biquad_process(biquad_t *s, float x)
{
    float y = s->b0*x + s->z1;
    s->z1 = s->b1*x - s->a1*y + s->z2;
    s->z2 = s->b2*x - s->a2*y;
    return y;
}

typedef struct {
    float a, b;      // adaptive weights
    float mu;
    float eps;

    // circular buffer for fractional delay
    float dly[64];
    int   di;

    // frequency tracking (very lightweight)
    float fs;
    float f_est;
    int   scount;
    int   last_sign;

    biquad_t bpf_com;
    biquad_t bpf_diff;

    float w_max;      // clamp for adaptive weights
    float com_lim;    // limiter for com50 and Q reference (same units as com)
    float diff_sat;   // if |diff| exceeds this, freeze adaptation (likely clipping)
    float com_sat;    // if |com| exceeds this, freeze adaptation (likely clipping)
    float leak;       // small leakage to slowly relax weights when conditions change

} anc_iq_t;


static inline float clampf(float x, float lo, float hi)
{
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

// soft limiter: linear near 0, saturates smoothly
static inline float soft_limit(float x, float lim)
{
    if (lim <= 0.0f) return x;
    return lim * tanhf(x / lim);
}


static inline void anc_iq_init(anc_iq_t *s, float fs)
{
    s->a = 0; s->b = 0;
    s->mu = 0.05f;     // اگر پمپ زد: 0.02
    s->eps = 1e-6f;

    for (int i=0;i<64;i++) s->dly[i]=0;
    s->di = 0;

    s->fs = fs;
    s->f_est = 50.0f;   // شروع
    s->scount = 0;
    s->last_sign = 0;

    //////////////////////////////////////////////////////////////////////
    s->w_max    = 4.0f;     // وزن‌ها معمولاً حوالی 1 هستند؛ 4 حاشیه خوبه
    s->com_lim  = 800.0f;   // mV: مرجع رو سالم نگه می‌داریم
    s->diff_sat = 1100.0f;  // mV: نزدیک ±1200mV فول‌اسکیل -> احتمال کلیپ
    s->com_sat  = 1100.0f;  // mV: همین منطق برای کانال مرجع
    s->leak     = 0.0005f;  // نشتی آهسته برای برگشت وزن‌ها
    ///////////////////////////////////////////////////////////////////////

    // Q را کمتر می‌کنیم تا drift فرکانس کمتر اذیت کند
    biquad_init_bandpass(&s->bpf_com,  fs, 50.0f, 10.0f);
    biquad_init_bandpass(&s->bpf_diff, fs, 50.0f, 10.0f);
}

// fractional-delay read from circular buffer
static inline float frac_delay_read(const float *buf, int len, int write_idx, float D)
{
    // want sample D samples ago: index = write_idx - D
    int i0 = (int)floorf(D);
    float frac = D - (float)i0;

    int idx0 = write_idx - i0;     // integer part
    int idx1 = idx0 - 1;           // one more older

    while (idx0 < 0) idx0 += len;
    while (idx1 < 0) idx1 += len;
    idx0 %= len;
    idx1 %= len;

    // linear interpolation
    float x0 = buf[idx0];
    float x1 = buf[idx1];
    return x0*(1.0f-frac) + x1*frac;
}

static inline float anc_iq_process(anc_iq_t *s, float com, float diff)
{
    // اگر نزدیک فول‌اسکیل باشیم، احتمال کلیپ/دیستورشن هست => تطبیق رو فریز کن
    bool freeze = (fabsf(diff) >= s->diff_sat) || (fabsf(com) >= s->com_sat);

    // 1) فقط مولفه‌ی 50Hz را جدا کن
    float com50  = biquad_process(&s->bpf_com,  com);
    float diff50 = biquad_process(&s->bpf_diff, diff);

    // 2) محدودسازی نرم مرجع (و اختیاری diff50 برای پایداری)
    com50  = soft_limit(com50,  s->com_lim);
    diff50 = soft_limit(diff50, s->com_lim);

    // 3) دنبال‌کردن خیلی سبک فرکانس با zero-crossing روی com50
    int sign = (com50 >= 0.0f) ? 1 : -1;
    s->scount++;

    if (s->last_sign < 0 && sign > 0) {
        if (s->scount > 5) {
            float f_meas = s->fs / (float)s->scount;
            s->f_est = 0.98f*s->f_est + 0.02f*f_meas;
        }
        s->scount = 0;
    }
    s->last_sign = sign;

    // 4) ساخت Q با تاخیر یک‌چهارم پریود (fractional delay)
    float D = s->fs / (4.0f * s->f_est);

    s->dly[s->di] = com50;
    float q = frac_delay_read(s->dly, 64, s->di, D);
    q = soft_limit(q, s->com_lim);

    s->di++; if (s->di >= 64) s->di = 0;

    // 5) leakage: اگر شرایط بد شد، آروم وزن‌ها رو به سمت صفر هل بده
    s->a *= (1.0f - s->leak);
    s->b *= (1.0f - s->leak);

    // clamp وزن‌ها برای جلوگیری از runaway
    s->a = clampf(s->a, -s->w_max, s->w_max);
    s->b = clampf(s->b, -s->w_max, s->w_max);

    // 6) پیش‌بینی 50Hz در diff و NLMS (اگر freeze نیست)
    float yhat50 = s->a*com50 + s->b*q;
    float e50    = diff50 - yhat50;

    float p = com50*com50 + q*q + s->eps;

    if (!freeze) {
        float g = (s->mu * e50) / p;
        s->a += g * com50;
        s->b += g * q;

        s->a = clampf(s->a, -s->w_max, s->w_max);
        s->b = clampf(s->b, -s->w_max, s->w_max);
    }

    // 7) کم کردن فقط همان مولفه از diff اصلی
    return diff - yhat50;
}