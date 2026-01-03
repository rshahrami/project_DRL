#include "notch50_iq.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline void iq_update(float* I, float* Q, float x, float c, float s, float alpha) {
    // 1st-order low-pass on mixed-down I/Q
    *I = (1.0f - alpha) * (*I) + alpha * (x * c);
    *Q = (1.0f - alpha) * (*Q) + alpha * (x * s);
}

static inline void iq_amp_phase(float I, float Q, float* A, float* phi) {
    *A   = 2.0f * sqrtf(I*I + Q*Q);
    *phi = atan2f(Q, I);
}

void notch50iq_init(Notch50IQ* st, float fs, float f0, float alpha) {
    if (!st) return;
    st->fs = fs;
    st->f0 = f0;
    st->alpha = alpha;
    notch50iq_reset(st);
}

void notch50iq_reset(Notch50IQ* st) {
    if (!st) return;
    st->theta = 0.0f;
    st->Ic = st->Qc = 0.0f;
    st->Id = st->Qd = 0.0f;
}

float notch50iq_process(Notch50IQ* st, float com, float diff) {
    if (!st) return diff;

    // oscillator
    float c = cosf(st->theta);
    float s = sinf(st->theta);

    // update IQ for com & diff at f0
    iq_update(&st->Ic, &st->Qc, com,  c, s, st->alpha);
    iq_update(&st->Id, &st->Qd, diff, c, s, st->alpha);

    // estimate amp/phase for diff 50Hz component
    float Ad, phid;
    iq_amp_phase(st->Id, st->Qd, &Ad, &phid);

    // synthesize 50Hz component in diff and subtract
    float d50_hat = Ad * sinf(st->theta + phid);
    float clean = diff - d50_hat;

    // advance phase
    float w = 2.0f * (float)M_PI * st->f0 / st->fs;
    st->theta += w;
    if (st->theta > 2.0f * (float)M_PI) st->theta -= 2.0f * (float)M_PI;

    return clean;
}

void notch50iq_get_estimates(const Notch50IQ* st,
                            float* Ac, float* phic,
                            float* Ad, float* phid) {
    if (!st) return;

    if (Ac || phic) {
        float A, phi;
        iq_amp_phase(st->Ic, st->Qc, &A, &phi);
        if (Ac) *Ac = A;
        if (phic) *phic = phi;
    }

    if (Ad || phid) {
        float A, phi;
        iq_amp_phase(st->Id, st->Qd, &A, &phi);
        if (Ad) *Ad = A;
        if (phid) *phid = phi;
    }
}
