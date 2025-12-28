#pragma once
#include <stdint.h>
#include <string.h>

#ifndef ANC_MAX_TAPS
#define ANC_MAX_TAPS 128
#endif

typedef struct {
    int L;
    float mu;
    float eps;
    float leak;           // 0.0..0.001 (برای جلوگیری از runaway)
    float w[ANC_MAX_TAPS];
    float x[ANC_MAX_TAPS];
    uint8_t adapt;
} anc_fir_t;

static inline void anc_fir_init(anc_fir_t *a, int taps, float mu, float leak)
{
    if (taps > ANC_MAX_TAPS) taps = ANC_MAX_TAPS;
    a->L = taps;
    a->mu = mu;
    a->eps = 1e-6f;
    a->leak = leak;      // مثلاً 1e-4
    a->adapt = 1;
    memset(a->w, 0, sizeof(a->w));
    memset(a->x, 0, sizeof(a->x));
}

static inline float anc_fir_process(anc_fir_t *a, float com, float diff)
{
    // delay line
    for (int i = a->L - 1; i > 0; --i) a->x[i] = a->x[i - 1];
    a->x[0] = com;

    // dot + power
    float yhat = 0.0f;
    float p = a->eps;
    for (int i = 0; i < a->L; ++i) {
        yhat += a->w[i] * a->x[i];
        p    += a->x[i] * a->x[i];
    }

    float e = diff - yhat;

    if (a->adapt) {
        float g = (a->mu * e) / p;
        for (int i = 0; i < a->L; ++i) {
            // leaky NLMS برای جلوگیری از رشد وزن‌ها
            a->w[i] = (1.0f - a->leak) * a->w[i] + g * a->x[i];
        }
    }
    return e;
}
