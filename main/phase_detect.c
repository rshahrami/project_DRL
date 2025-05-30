//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdint.h>
#include <math.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define e 0.000000000001
#define PI 3.141592653589793f
#define SAMPLE_RATE 1000000
#define SIGNAL_FREQ 50
const uint32_t PERIOD_SAMPLES = SAMPLE_RATE / SIGNAL_FREQ;
const float SAMPLES_PER_DEGREE = (float)PERIOD_SAMPLES / 360.0f;

float calculate_phase_diff(float s1, float s2) {
    static float prev_s1 = 0.0f, prev_s2 = 0.0f;
    static int32_t last_zc1 = -1;
    static int32_t last_zc2 = -1;
    static uint32_t sample_counter = 0;

    sample_counter++;

    // Detect zero crossing for signal 1
    if (prev_s1 <= 0 && s1 > 0) {
        last_zc1 = sample_counter;
    }

    // Detect zero crossing for signal 2
    if (prev_s2 <= 0 && s2 > 0) {
        last_zc2 = sample_counter;
    }

    prev_s1 = s1;
    prev_s2 = s2;

    if (last_zc1 >= 0 && last_zc2 >= 0) {
        int32_t delta_samples = last_zc1 - last_zc2;

        // Normalize to range [-PERIOD_SAMPLES/2, +PERIOD_SAMPLES/2]
        while (delta_samples > (int32_t)(PERIOD_SAMPLES / 2)) {
            delta_samples -= PERIOD_SAMPLES;
        }
        while (delta_samples < -(int32_t)(PERIOD_SAMPLES / 2)) {
            delta_samples += PERIOD_SAMPLES;
        }

        float phase_deg = (float)delta_samples / SAMPLES_PER_DEGREE;

        return phase_deg;
    }

    return -1000;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
