#ifndef MAX_VALUE_H
#define MAX_VALUE_H

#include <stdbool.h>

typedef struct {
    float prev;
    float max_val;
    bool increasing;
} PeakTracker;

void initPeakTracker(PeakTracker* tracker);
float updatePeak(PeakTracker* tracker, float signal);

#endif // MAX_VALUE_H
