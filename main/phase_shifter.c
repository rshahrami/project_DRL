// #include <math.h>
// #include <string.h>

// #ifndef M_PI
// #define M_PI 3.14159265358979323846
// #endif

// #define FS_DEFAULT     1000.0f
// #define F0_DEFAULT       50.0f
// #define SPREAD_DEFAULT    8.0f

// typedef struct {
//     float a;
//     float x1, y1;
// } ap1_t;

// typedef struct {
//     float fs;
//     float f0;
//     float spread_hz;
//     float mix;   // 0..0.5 (0.5 = ناتچ کامل)
//     ap1_t ap1, ap2;
// } notch50_t;

// static notch50_t n50;

// static float alpha_from_f0(float fs, float f0)
// {
//     const float w0 = 2.0f * (float)M_PI * (f0 / fs);
//     const float s  = sinf(w0);
//     const float c  = cosf(w0);
//     return -(c - s) / (s + c);
// }

// static inline float ap1_step(ap1_t *st, float x)
// {
//     const float y = st->a * x + st->x1 - st->a * st->y1;
//     st->x1 = x; st->y1 = y;
//     return y;
// }

// static void retune_allpass_chain(notch50_t *st)
// {
//     const float f1 = st->f0 - 0.5f * st->spread_hz;
//     const float f2 = st->f0 + 0.5f * st->spread_hz;

//     const float f1c = (f1 > 1.0f) ? f1 : 1.0f;
//     const float f2c = (f2 < 0.45f * st->fs) ? f2 : 0.45f * st->fs;

//     st->ap1.a = alpha_from_f0(st->fs, f1c);
//     st->ap2.a = alpha_from_f0(st->fs, f2c);

//     st->ap1.x1 = st->ap1.y1 = 0.0f;
//     st->ap2.x1 = st->ap2.y1 = 0.0f;
// }

// void notch50_init(float fs, float f0, float spread_hz, float depth_full_0_to_1)
// {
//     n50.fs        = (fs        > 0.0f) ? fs        : FS_DEFAULT;
//     n50.f0        = (f0        > 0.0f) ? f0        : F0_DEFAULT;
//     n50.spread_hz = (spread_hz >= 0.0f) ? spread_hz : SPREAD_DEFAULT;

//     if (depth_full_0_to_1 < 0.0f) depth_full_0_to_1 = 0.0f;
//     if (depth_full_0_to_1 > 1.0f) depth_full_0_to_1 = 1.0f;
//     n50.mix = 0.5f * depth_full_0_to_1;  // 0..0.5

//     memset(&n50.ap1, 0, sizeof(n50.ap1));
//     memset(&n50.ap2, 0, sizeof(n50.ap2));
//     retune_allpass_chain(&n50);
// }

// void notch50_set_freq(float f0_hz)
// {
//     if (f0_hz <= 0.0f) return;
//     n50.f0 = f0_hz;
//     retune_allpass_chain(&n50);
// }

// void notch50_set_spread(float spread_hz)
// {
//     if (spread_hz < 0.0f) spread_hz = 0.0f;
//     n50.spread_hz = spread_hz;
//     retune_allpass_chain(&n50);
// }

// void notch50_set_depth(float depth_full_0_to_1)
// {
//     if (depth_full_0_to_1 < 0.0f) depth_full_0_to_1 = 0.0f;
//     if (depth_full_0_to_1 > 1.0f) depth_full_0_to_1 = 1.0f;
//     n50.mix = 0.5f * depth_full_0_to_1;
// }

// float notch50_process_sample(float x)
// {
//     float q = ap1_step(&n50.ap1, x);
//     q = ap1_step(&n50.ap2, q);
//     const float k = n50.mix;   // 0..0.5
//     return (1.0f - k) * x + k * q;
// }

// void notch50_process_block(const float *in, float *out, int n)
// {
//     for (int i = 0; i < n; ++i)
//         out[i] = notch50_process_sample(in[i]);
// }



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



#include <math.h>
#include <stdint.h>

#define PI 3.14159265358979323846

// تنظیمات
#define F_SIGNAL    50.0        // Hz
#define F_SAMPLING  1000.0   // Hz  (اگر 1kHz داری، همین را 1000.0 بگذار)

static float filter_alpha   = 0.0f;
static float filter_x_prev  = 0.0f;
static float filter_y_prev  = 0.0f;

static float current_gain_a = 1.0f;
static float current_gain_b = 0.0f;

// آل‌پس مرتبه‌اول: H(z)=(a+z^-1)/(1+a z^-1)
// شرط فاز ≈ -90° در ω0:  a = -(cosω0 - sinω0)/(sinω0 + cosω0)
void init_phase_shifter(void) {
    const double w0 = 2.0 * PI * (F_SIGNAL / F_SAMPLING);
    const double s  = sin(w0);
    const double c  = cos(w0);
    filter_alpha = (float)(-(c - s) / (s + c));   // برای Fs=1MHz → حدود -0.99937
    filter_x_prev = 0.0f;
    filter_y_prev = 0.0f;
}

// شیفت فاز دلخواه (0..180°)
void set_phase_shift_degrees(float deg) {
    while (deg > 180.0f)  deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    float ph = deg * (float)(PI / 180.0);
    current_gain_a = cosf(ph);
    current_gain_b = -sinf(ph);
}

// پردازش یک نمونه
float process_sample(float x_n) {
    // y[n] = alpha*x[n] + x[n-1] - alpha*y[n-1]   (all-pass)
    const float y_n = filter_alpha * x_n + filter_x_prev - filter_alpha * filter_y_prev;
    filter_x_prev = x_n;
    filter_y_prev = y_n;

    // خروجی با شیفت فاز هدف: a*I + b*Q
    return current_gain_a * x_n + current_gain_b * y_n;
}
