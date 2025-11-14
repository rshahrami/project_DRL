#pragma once

#include <stdint.h>

// اعلان توابع phase shifter
void init_phase_shifter(void);
void set_phase_shift_degrees(float deg);
float process_sample(float x_n);
