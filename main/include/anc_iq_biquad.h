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
} anc_iq_t;

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
    // 1) فقط مولفه‌ی 50Hz را جدا کن
    float com50  = biquad_process(&s->bpf_com,  com);
    float diff50 = biquad_process(&s->bpf_diff, diff);

    // 2) فرکانس را خیلی سبک از zero-crossing روی com50 دنبال کن (slow drift)
    int sign = (com50 >= 0.0f) ? 1 : -1;
    s->scount++;

    // positive-going crossing: (-) -> (+)
    if (s->last_sign < 0 && sign > 0) {
        if (s->scount > 5) {
            float f_meas = s->fs / (float)s->scount;
            // LPF روی تخمین فرکانس
            s->f_est = 0.98f*s->f_est + 0.02f*f_meas;
        }
        s->scount = 0;
    }
    s->last_sign = sign;

    // 3) ساخت Q با تاخیر یک‌چهارم پریود، ولی اعشاری و adaptive:
    // D = fs/(4*f_est)
    float D = s->fs / (4.0f * s->f_est);   // حدود 20 ولی کمی تغییر می‌کند

    // write current sample
    s->dly[s->di] = com50;

    // read delayed (fractional) sample
    float q = frac_delay_read(s->dly, 64, s->di, D);

    s->di++; if (s->di >= 64) s->di = 0;

    // 4) پیش‌بینی 50Hz در diff و NLMS
    float yhat50 = s->a*com50 + s->b*q;
    float e50 = diff50 - yhat50;

    float p = com50*com50 + q*q + s->eps;
    float g = (s->mu * e50) / p;
    s->a += g * com50;
    s->b += g * q;

    // 5) کم کردن فقط همان مولفه از diff اصلی
    return diff - yhat50;
}
