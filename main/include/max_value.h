#ifndef MAX_VALUE_H
#define MAX_VALUE_H

#include <stdbool.h>

/* ساختار ردیاب قله 50Hz */
typedef struct {
    float max_val;        // ماکس سیکل جاری
    float last_peak;      // آخرین peak معتبر 50Hz
    float prev;           // نمونه قبلی
    bool  first;          // برای اولین نمونه
} PeakTracker;

/* API */
void initPeakTracker(PeakTracker* tracker);
float updatePeak50Hz(PeakTracker* tracker, float signal);

#endif // MAX_VALUE_H
