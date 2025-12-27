// iq_subtract_nco.c
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE_HZ 7812.0f
#define SIGNAL_FREQ    50.0f

typedef struct {
    float i;
    float q;
    float alpha;
    float phase;   // مرحله NCO
    float omega;   // 2π f / Fs
} NCOTracker;

void init_nco(NCOTracker *t, float lpf_alpha) {
    t->i = t->q = 0.0f;
    t->phase = 0.0f;
    t->alpha = lpf_alpha;
    t->omega = 2.0f * M_PI * SIGNAL_FREQ / SAMPLE_RATE_HZ;
}

// diff = signal, returns diff_clean
float iq_subtract_nco(NCOTracker *t, float diff) {
    // 1- NCO
    float ref_cos = cosf(t->phase);
    float ref_sin = sinf(t->phase);
    t->phase += t->omega;
    if (t->phase >= 2.0f * M_PI) t->phase -= 2.0f * M_PI;

    // 2- دمدولاسیون I/Q
    float xi = diff * ref_cos;
    float xq = diff * ref_sin;

    // 3- LPF
    t->i = t->i + t->alpha * (xi - t->i);
    t->q = t->q + t->alpha * (xq - t->q);

    // 4- بازسازی 50Hz
    float diff_50hz = t->i * ref_cos + t->q * ref_sin;

    // 5- حذف
    return diff - diff_50hz;
}
