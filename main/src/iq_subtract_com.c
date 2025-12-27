// iq_subtract_com.c
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE_HZ 7812.0f   // ADC rate
#define SIGNAL_FREQ    50.0f

// تک‌قطبی LPF
static inline float lpf(float y, float x, float alpha) {
    return y + alpha * (x - y);
}

typedef struct {
    float i;      // مولفه I
    float q;      // مولفه Q
    float alpha;  // LPF coefficient
} IQTracker;

// init
void init_iqtracker(IQTracker *t, float lpf_alpha) {
    t->i = t->q = 0.0f;
    t->alpha = lpf_alpha;
}

// dmdoule diff with com as reference
float iq_subtract_com(IQTracker *tracker, float com, float diff) {
    // 1- normalize com
    float com_norm = com / 1200.0f; // یا هر دامنه ماکزیمم واقعی
    if (com_norm > 1.0f) com_norm = 1.0f;
    if (com_norm < -1.0f) com_norm = -1.0f;

    // 2- generate reference I/Q
    float ref_cos = com_norm;
    float ref_sin = sinf(acosf(com_norm)); 

    // 3- dmdoule diff
    float xi = diff * ref_cos;
    float xq = diff * ref_sin;

    // 4- LPF برای جدا کردن 50Hz
    tracker->i = lpf(tracker->i, xi, tracker->alpha);
    tracker->q = lpf(tracker->q, xq, tracker->alpha);

    // 5- بازسازی مولفه 50Hz
    float diff_50hz = tracker->i * ref_cos + tracker->q * ref_sin;

    // 6- حذف مولفه 50Hz
    float diff_clean = diff - diff_50hz;

    return diff_clean;
}
