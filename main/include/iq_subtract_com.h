#ifndef IQ_SUBTRACT_COM_H
#define IQ_SUBTRACT_COM_H

typedef struct {
    float i;      // مولفه I
    float q;      // مولفه Q
    float alpha;  // LPF coefficient
} IQTracker;

// مقدار alpha ≈ 0.05–0.1 برای LPF
void init_iqtracker(IQTracker *t, float lpf_alpha);

// diff: سیگنال ورودی diff
// com: سیگنال مرجع com
// خروجی: diff بدون مولفه 50Hz
float iq_subtract_com(IQTracker *tracker, float com, float diff);

#endif
