#include "max_value.h"

void initPeakTracker(PeakTracker* tracker) {
    tracker->prev = 0.0f;
    tracker->max_val = 0.0f;
    tracker->increasing = false;
    tracker->started = false;
}


float updatePeak(PeakTracker* tracker, float signal) {
    if (!tracker->started) {
        tracker->prev = signal;
        tracker->started = true;
        tracker->increasing = false;
        return tracker->max_val; // هنوز قله‌ای نداریم
    }

    if (signal > tracker->prev) {
        tracker->prev = signal;
        tracker->increasing = true;
    } else if (tracker->increasing) {
        // تغییر از افزایش به کاهش => قله یافت شد
        tracker->max_val = tracker->prev;
        tracker->increasing = false;
        tracker->prev = signal;
    } else {
        tracker->prev = signal;
    }
    return tracker->max_val;
}


// float updatePeak(PeakTracker* tracker, float signal) {
    // if (signal > tracker->prev) {
    //     tracker->prev = signal;
    //     tracker->started = true;
    //     tracker->increasing = true;
//     } else if (tracker->increasing) {
//         tracker->max_val = tracker->prev;
//         tracker->increasing = false;
//         tracker->prev = signal;
//     } else {
//         tracker->prev = signal;
//     }
//     return tracker->max_val;
// }
// 