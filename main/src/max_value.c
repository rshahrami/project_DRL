#include "max_value.h"



void initPeakTracker(PeakTracker* tracker)
{
    tracker->max_val   = 0.0f;
    tracker->last_peak = 0.0f;
    tracker->prev      = 0.0f;
    tracker->first     = true;
}

float updatePeak50Hz(PeakTracker* tracker, float signal)
{
    if (tracker->first) {
        tracker->prev = signal;
        tracker->first = false;
        return tracker->last_peak;
    }

    /* دنبال بیشینه در سیکل جاری */
    if (signal > tracker->max_val) {
        tracker->max_val = signal;
    }

    /* تشخیص zero-crossing منفی → مثبت */
    if (tracker->prev < 0.0f && signal >= 0.0f) {
        // پایان یک سیکل کامل
        tracker->last_peak = tracker->max_val;

        // ریست برای سیکل بعدی
        tracker->max_val = 0.0f;
    }

    tracker->prev = signal;
    return tracker->last_peak;
}


//////////////////////////////////////////////////////////////////////////




// #include "max_value.h"

// void initPeakTracker(PeakTracker* tracker) {
//     tracker->prev = 0.0f;
//     tracker->max_val = 0.0f;
//     tracker->increasing = false;
//     tracker->started = false;
// }


// float updatePeak(PeakTracker* tracker, float signal) {
//     if (!tracker->started) {
//         tracker->prev = signal;
//         tracker->started = true;
//         tracker->increasing = false;
//         return tracker->max_val; // هنوز قله‌ای نداریم
//     }

//     if (signal > tracker->prev) {
//         tracker->prev = signal;
//         tracker->increasing = true;
//     } else if (tracker->increasing) {
//         // تغییر از افزایش به کاهش => قله یافت شد
//         tracker->max_val = tracker->prev;
//         tracker->increasing = false;
//         tracker->prev = signal;
//     } else {
//         tracker->prev = signal;
//     }
//     return tracker->max_val;
// }
