#pragma once
#include <stdint.h>

typedef struct {
    float w0, w1;       // weights
    float mu;           // step size
    float eps;          // small number for stability
    float com_prev;     // for derivative term
} anc2_t;

static inline void anc2_init(anc2_t *a, float mu)
{
    a->w0 = 0.0f;
    a->w1 = 0.0f;
    a->mu = mu;
    a->eps = 1e-6f;
    a->com_prev = 0.0f;
}

// returns diff_clean
static inline float anc2_process(anc2_t *a, float com, float diff)
{
    // Two reference components:
    // x0 ~ cos(ωt) component, x1 ~ sin(ωt) component (approx via derivative)
    float x0 = com;
    float x1 = com - a->com_prev;
    a->com_prev = com;

    // Interference estimate in diff
    float yhat = a->w0 * x0 + a->w1 * x1;

    // Error = cleaned signal
    float e = diff - yhat;

    // Normalized LMS update
    float p = x0*x0 + x1*x1 + a->eps;
    float g = (a->mu * e) / p;

    a->w0 += g * x0;
    a->w1 += g * x1;

    return e;
}
