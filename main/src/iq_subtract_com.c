// iq_cancel_50hz.c  (جایگزین iq_subtract_com.c)
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    // PLL
    float fs;
    float f0;        // target ~50Hz
    float theta;     // phase
    float omega;     // rad/sample
    float pll_kp;
    float pll_ki;
    float integ;

    // Amplitude/phase estimator (NLMS 2-tap)
    float wc;        // weight for cos
    float ws;        // weight for sin
    float mu;        // step size (NLMS)
    float eps;       // avoid divide-by-zero

    // optional: com bandpass-ish (simple HP/LP pair)
    float com_dc;    // remove DC
    float com_hp;
} Cancel50;

static inline float wrap_pi(float x) {
    while (x >  (float)M_PI) x -= 2.0f*(float)M_PI;
    while (x < -(float)M_PI) x += 2.0f*(float)M_PI;
    return x;
}

static inline float lpf1(float y, float x, float alpha) {
    return y + alpha * (x - y);
}

// init
void cancel50_init(Cancel50 *c, float fs_hz)
{
    c->fs = fs_hz;
    c->f0 = 50.0f;

    c->theta = 0.0f;
    c->omega = 2.0f*(float)M_PI * (c->f0 / c->fs);

    // PLL gains (شروع محافظه‌کارانه)
    // اگر com نویزی است، کوچک‌تر کن.
    c->pll_kp = 0.02f;
    c->pll_ki = 0.0002f;
    c->integ  = 0.0f;

    // NLMS
    c->wc  = 0.0f;
    c->ws  = 0.0f;
    c->mu  = 0.02f;      // اگر ناپایدار شد 0.005..0.01
    c->eps = 1e-6f;

    // DC removal for com (خیلی ملایم)
    c->com_dc = 0.0f;
    c->com_hp = 0.0f;
}

// یک فیلتر خیلی ساده برای com: حذف DC (و کمی کمک به PLL)
static inline float com_preprocess(Cancel50 *c, float com)
{
    // com_dc LPF خیلی آهسته (alpha خیلی کوچک)
    c->com_dc = lpf1(c->com_dc, com, 0.001f);
    float hp = com - c->com_dc;
    return hp;
}

// main: returns diff_clean
float cancel50_process(Cancel50 *c, float com, float diff)
{
    // 1) preprocess com (DC removal)
    float x = com_preprocess(c, com);

    // 2) NCO outputs
    float cs = cosf(c->theta);
    float sn = sinf(c->theta);

    // 3) PLL phase detector: ضرب com در sin (یا -sin) برای خطای فاز
    // وقتی x هم‌فاز با cos باشد، x*sn ~ خطا
    float phase_err = x * sn;

    // 4) PLL loop filter
    c->integ += c->pll_ki * phase_err;
    float d_omega = c->pll_kp * phase_err + c->integ;

    // محدودسازی برای جلوگیری از فرار PLL (±2Hz مثلاً)
    float max_dev = 2.0f*(float)M_PI * (2.0f / c->fs);
    if (d_omega >  max_dev) d_omega =  max_dev;
    if (d_omega < -max_dev) d_omega = -max_dev;

    // 5) update phase
    c->theta = wrap_pi(c->theta + c->omega + d_omega);

    // 6) Recompute sin/cos after phase update (می‌تونی حذفش کنی برای سرعت)
    cs = cosf(c->theta);
    sn = sinf(c->theta);

    // 7) NLMS estimate of 50Hz component in diff
    float yhat = c->wc * cs + c->ws * sn;
    float e = diff - yhat;

    // NLMS update
    float norm = (cs*cs + sn*sn) + c->eps; // تقریباً 1
    float g = (c->mu * e) / norm;
    c->wc += g * cs;
    c->ws += g * sn;

    // 8) output clean
    return e;
}
