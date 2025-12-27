#ifndef IQ_SUBTRACT_NCO_H
#define IQ_SUBTRACT_NCO_H

typedef struct {
    float i;      // مولفه I
    float q;      // مولفه Q
    float alpha;  // LPF coefficient
    float phase;  // مرحله NCO
    float omega;  // 2π f / Fs
} NCOTracker;

// مقدار alpha ≈ 0.05–0.1 برای LPF
void init_nco(NCOTracker *t, float lpf_alpha);

// diff: سیگنال ورودی diff
// خروجی: diff بدون مولفه 50Hz
float iq_subtract_nco(NCOTracker *t, float diff);

#endif
