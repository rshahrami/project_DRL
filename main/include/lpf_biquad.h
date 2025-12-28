#pragma once
#include <math.h>

typedef struct {
    float b0,b1,b2,a1,a2;
    float z1,z2;
} lpf_biquad_t;

static inline float lpf_biquad_process(lpf_biquad_t *s, float x)
{
    float y = s->b0*x + s->z1;
    s->z1 = s->b1*x - s->a1*y + s->z2;
    s->z2 = s->b2*x - s->a2*y;
    return y;
}

// 2nd-order Lowpass (RBJ cookbook). Q=0.707 => Butterworth-ish
static inline void lpf_biquad_init_lowpass(lpf_biquad_t *s, float fs, float fc, float Q)
{
    float w0 = 2.0f * (float)M_PI * (fc / fs);
    float c  = cosf(w0);
    float sn = sinf(w0);
    float alpha = sn / (2.0f * Q);

    float b0 = (1.0f - c) * 0.5f;
    float b1 =  1.0f - c;
    float b2 = (1.0f - c) * 0.5f;
    float a0 =  1.0f + alpha;
    float a1 = -2.0f * c;
    float a2 =  1.0f - alpha;

    s->b0 = b0/a0; s->b1 = b1/a0; s->b2 = b2/a0;
    s->a1 = a1/a0; s->a2 = a2/a0;
    s->z1 = 0.0f;  s->z2 = 0.0f;
}

// 4th-order LPF = دو تا 2nd-order پشت سر هم
typedef struct {
    lpf_biquad_t s1;
    lpf_biquad_t s2;
} lpf4_t;

static inline void lpf4_init(lpf4_t *f, float fs, float fc)
{
    lpf_biquad_init_lowpass(&f->s1, fs, fc, 0.70710678f);
    lpf_biquad_init_lowpass(&f->s2, fs, fc, 0.70710678f);
}

static inline float lpf4_process(lpf4_t *f, float x)
{
    x = lpf_biquad_process(&f->s1, x);
    x = lpf_biquad_process(&f->s2, x);
    return x;
}
