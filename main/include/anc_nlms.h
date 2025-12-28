#pragma once
#include <math.h>
#include <stdint.h>

typedef struct {
    float w0, w1;
    float mu;
    float eps;
    float com_prev;
    float k_der;     // fs/(2*pi*f0)
    uint8_t adapt;   // 1=یادگیری فعال، 0=فقط کم کن
} anc2_t;

static inline void anc2_init(anc2_t *a, float fs, float f0, float mu)
{
    a->w0 = 0.0f;
    a->w1 = 0.0f;
    a->mu = mu;          // پیشنهاد: 0.001 تا 0.004
    a->eps = 1e-6f;
    a->com_prev = 0.0f;
    a->k_der = fs / (2.0f * (float)M_PI * f0);
    a->adapt = 1;
}

static inline float anc2_process(anc2_t *a, float com, float diff)
{
    float x0 = com;
    float x1 = (com - a->com_prev) * a->k_der; // <-- نرمال‌سازی مشتق
    a->com_prev = com;

    float yhat = a->w0 * x0 + a->w1 * x1;
    float e = diff - yhat;

    if (a->adapt) {
        float p = x0*x0 + x1*x1 + a->eps;
        float g = (a->mu * e) / p;
        a->w0 += g * x0;
        a->w1 += g * x1;
    }
    return e;
}
