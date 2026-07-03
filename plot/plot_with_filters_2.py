import serial
import re
import numpy as np
import matplotlib.pyplot as plt

# =========================
# تنظیمات پورت و نرخ ارسال
# =========================
PORT = 'COM3'
BAUD = 921600

# =========================
# تنظیمات پنجره و تحلیل
# =========================
REAL_SAMPLES_WINDOW = 5000   # مثلاً سه سیکل 1Hz در 1kSPS
WARMUP_SKIP = 0             # اگر ESP خودش warmup را چاپ نمی‌کند، اینجا می‌توانی n اولیه را نادیده بگیری

# =========================
# نرخ نمونه‌برداری واقعی ADC (Hz)
# (برای طراحی فیلترها لازم است)
# =========================
SAMPLE_RATE_HZ = 1000.0

SKIP_INITIAL_SAMPLES = 100   # قابل تنظیم

# =========================
# تنظیمات فیلترها
# =========================
USE_NOTCH_50HZ = False
NOTCH_F0_HZ = 50.0
NOTCH_Q = 30.0           # بزرگ‌تر => ناچ باریک‌تر

USE_LOWPASS = False
LOWPASS_FC_HZ = 50.0    # فرکانس قطع (Hz)
LOWPASS_Q = 0.70710678   # Q ~ 0.707 (Butterworth)


TEST_MODE = "diff"   # "common" یا "diff"
# اگر خروجی تو float mV است:
MODE = "float_mV"
# اگر خروجی را به int (mV*1000) تبدیل کردی، این را بگذار:
# MODE = "int_uV_in_mVx1000"
...

# =========================
# فیلترها (Biquad IIR)
# =========================
def _biquad_df2(x, b, a):
    """
    Direct Form II Transposed biquad.
    x: 1D array
    b: [b0,b1,b2]
    a: [1,a1,a2]  (a0 must be 1)
    """
    x = np.asarray(x, dtype=float)
    y = np.empty_like(x, dtype=float)

    s1 = 0.0
    s2 = 0.0

    b0, b1, b2 = b
    _, a1, a2 = a

    for i in range(len(x)):
        xi = x[i]
        # اگر NaN داری (برای break کردن نمودار)، همون NaN رو عبور بده و state رو دست نزن
        if np.isnan(xi):
            y[i] = np.nan
            continue

        yi = b0 * xi + s1
        s1 = b1 * xi - a1 * yi + s2
        s2 = b2 * xi - a2 * yi
        y[i] = yi

    return y


def notch_filter(x, fs, f0=50.0, Q=30.0):
    """
    Notch around f0 with quality factor Q.
    هرچی Q بزرگ‌تر => ناچ باریک‌تر (کمتر به اطراف آسیب می‌زنه).
    """
    x = np.asarray(x, dtype=float)
    f0 = float(f0)
    fs = float(fs)
    Q = float(Q)

    w0 = 2.0 * np.pi * (f0 / fs)
    alpha = np.sin(w0) / (2.0 * Q)

    b0 = 1.0
    b1 = -2.0 * np.cos(w0)
    b2 = 1.0

    a0 = 1.0 + alpha
    a1 = -2.0 * np.cos(w0)
    a2 = 1.0 - alpha

    b = np.array([b0, b1, b2], dtype=float) / a0
    a = np.array([1.0, a1 / a0, a2 / a0], dtype=float)

    return _biquad_df2(x, b, a)


def lowpass_filter(x, fs, fc, Q=0.70710678):
    """
    2nd-order lowpass biquad (RBJ cookbook). Q پیش‌فرض ~ 0.707 = Butterworth.
    fc: فرکانس قطع (Hz)
    """
    x = np.asarray(x, dtype=float)
    fs = float(fs)
    fc = float(fc)
    Q = float(Q)

    nyq = fs / 2.0
    if fc <= 0:
        return np.zeros_like(x)
    if fc >= 0.999 * nyq:
        return x.copy()

    w0 = 2.0 * np.pi * (fc / fs)
    alpha = np.sin(w0) / (2.0 * Q)
    cosw0 = np.cos(w0)

    b0 = (1.0 - cosw0) / 2.0
    b1 = 1.0 - cosw0
    b2 = (1.0 - cosw0) / 2.0

    a0 = 1.0 + alpha
    a1 = -2.0 * cosw0
    a2 = 1.0 - alpha

    b = np.array([b0, b1, b2], dtype=float) / a0
    a = np.array([1.0, a1 / a0, a2 / a0], dtype=float)

    return _biquad_df2(x, b, a)


# =========================
# Regex و پارس کردن خروجی ESP32
# =========================
# حالت float:  n,com,diff,clean  (مثلاً: 18670,7.259,-50.089,-1.543)
# pat_float = re.compile(rb'^\s*(\d+)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*$')

pat_float = re.compile(rb'^\s*(\d+)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*$')

# حالت int: n,com_i,diff_i,clean_i (مثلاً mV*1000)
pat_int   = re.compile(rb'^\s*(\d+)\s*,\s*([-+]?\d+)\s*,\s*([-+]?\d+)\s*,\s*([-+]?\d+)\s*$')

ansi = re.compile(rb'\x1b\[[0-9;]*[A-Za-z]')

def is_log_line(b: bytes) -> bool:
    # هرچی شبیه لاگ‌های ESP-IDF یا پیام‌های خطا باشد را رد کن
    bad_prefixes = (
        b'ESP-ROM', b'ets ', b'Guru Meditation', b'abort', b'failed',
        b'E (', b'W (', b'I (', b'D ('
    )
    return any(b.startswith(p) for p in bad_prefixes)

def parse_data_line(b: bytes):
    """return (n, com, diff, clean) in mV as float, or None"""
    if MODE == "float_mV":
        m = pat_float.match(b)
        if not m:
            return None
        n         = int(m.group(1))
        com       = float(m.group(2))
        diff      = float(m.group(3))
        noise_est = float(m.group(4))
        clean     = float(m.group(5))

        return n, com, diff, noise_est, clean
        # n = int(m.group(1))
        # com = float(m.group(2))
        # diff = float(m.group(3))
        # clean = float(m.group(4))
        # return n, com, diff, clean

    elif MODE == "int_uV_in_mVx1000":
        m = pat_int.match(b)
        if not m:
            return None
        n = int(m.group(1))
        com_i = int(m.group(2))
        diff_i = int(m.group(3))
        clean_i = int(m.group(4))
        # اینجا فرض کردیم خروجی ESP = mV*1000 (یعنی واحد 0.001mV = 1uV)
        com = com_i / 1000.0
        diff = diff_i / 1000.0
        clean = clean_i / 1000.0
        return n, com, diff, clean

    else:
        raise ValueError("Unknown MODE")


# =========================
# خواندن دیتا از سریال
# =========================
n_list = []
com_list = []
diff_list = []
clean_list = []
noise_est_list = []

skipped_logs = 0
skipped_other = 0

n_start = None

print("Start reading...")

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    ser.reset_input_buffer()

    while True:
        raw = ser.readline()
        if not raw:
            continue

        # پاکسازی ANSI و trim
        raw2 = ansi.sub(b'', raw).strip()
        if not raw2:
            continue

        # لاگ‌ها را دور بریز
        if is_log_line(raw2):
            skipped_logs += 1
            continue

        parsed = parse_data_line(raw2)
        if parsed is None:
            skipped_other += 1
            continue

        # n, com, diff, clean = parsed
        n, com, diff, noise_est, clean = parsed

        # تعیین شروع پنجره
        if n_start is None:
            n_start = n + WARMUP_SKIP

        # اگر هنوز زیر n_start هستیم، رد کن
        if n < n_start:
            continue

        # ذخیره
        n_list.append(n)
        com_list.append(com)
        diff_list.append(diff)
        noise_est_list.append(noise_est)
        clean_list.append(clean)


        if len(n_list) >= REAL_SAMPLES_WINDOW:
            break

        # # شرط پایان: پنجره‌ی REAL_SAMPLES_WINDOW نمونه‌ی واقعی
        # if (n - n_start) >= REAL_SAMPLES_WINDOW:
        #     break

print("Stop.")
print(f"Captured printed lines: {len(n_list)}")
print(f"Real samples covered:  {n_list[-1] - n_start}")
print(f"Skipped log/error lines: {skipped_logs}")
print(f"Skipped other malformed lines: {skipped_other}")


# =========================
# تبدیل به آرایه
# =========================
n = np.array(n_list, dtype=np.int64)
com = np.array(com_list, dtype=float)
diff = np.array(diff_list, dtype=float)
noise_est = np.array(noise_est_list, dtype=float)
clean = np.array(clean_list, dtype=float)




# =========================
# حذف transient ابتدایی
# =========================


if len(n) > SKIP_INITIAL_SAMPLES:
    n     = n[SKIP_INITIAL_SAMPLES:]
    com   = com[SKIP_INITIAL_SAMPLES:]
    diff  = diff[SKIP_INITIAL_SAMPLES:]
    noise_est = noise_est[SKIP_INITIAL_SAMPLES:]
    clean = clean[SKIP_INITIAL_SAMPLES:]


def longest_continuous_segment(n, *signals, expected_step=1):
    dn = np.diff(n)

    if len(dn) == 0:
        return (n, *signals)

    segments = []
    start = 0

    for i, d in enumerate(dn):
        if d != expected_step:
            segments.append((start, i + 1))
            start = i + 1

    segments.append((start, len(n)))

    a, b = max(segments, key=lambda s: s[1] - s[0])

    print(f"\nCMRR/FFT uses continuous segment: {b-a} samples")
    print(f"Segment n: {n[a]} -> {n[b-1]}")

    return (n[a:b], *(sig[a:b] for sig in signals))

n_calc, com_calc, diff_calc, noise_est_calc, clean_calc = longest_continuous_segment(
    n, com, diff, noise_est, clean, expected_step=1
)

# =========================================================
# FFT amplitude @ 50Hz
# =========================================================
def amp_peak_near_freq(x, fs=1000.0, f0=50.0, bw=3.0):
    x = np.asarray(x, dtype=float)
    x = x[np.isfinite(x)]
    x = x - np.mean(x)

    N = len(x)
    win = np.hanning(N)

    X = np.fft.rfft(x * win)
    freqs = np.fft.rfftfreq(N, d=1.0/fs)
    amp = 2.0 * np.abs(X) / np.sum(win)

    mask = (freqs >= f0 - bw) & (freqs <= f0 + bw)
    idx = np.argmax(amp[mask])

    return freqs[mask][idx], amp[mask][idx]



# def sine_fit_amp(n, x, fs=1000.0, f0=50.0):
#     n = np.asarray(n, dtype=float)
#     x = np.asarray(x, dtype=float)

#     mask = np.isfinite(x)

#     n = n[mask]
#     x = x[mask]

#     t = n / fs
#     w = 2.0 * np.pi * f0

#     A = np.column_stack([
#         np.sin(w * t),
#         np.cos(w * t),
#         np.ones_like(t)
#     ])

#     coef, *_ = np.linalg.lstsq(A, x, rcond=None)

#     a, b, dc = coef

#     amp_peak = np.sqrt(a*a + b*b)

#     return amp_peak, dc



def sine_fit_amp_at_freq(n, x, fs=1000.0, f0=50.0):
    n = np.asarray(n, dtype=float)
    x = np.asarray(x, dtype=float)

    mask = np.isfinite(x)
    n = n[mask]
    x = x[mask]

    t = n / fs
    w = 2.0 * np.pi * f0

    A = np.column_stack([
        np.sin(w * t),
        np.cos(w * t),
        np.ones_like(t)
    ])

    coef, *_ = np.linalg.lstsq(A, x, rcond=None)
    a, b, dc = coef

    amp_peak = np.sqrt(a*a + b*b)
    return amp_peak, dc


def find_best_sine_freq(n, x, fs=1000.0, f_min=48.0, f_max=52.0, df=0.01):
    freqs = np.arange(f_min, f_max + df, df)
    amps = []

    for f in freqs:
        amp, _ = sine_fit_amp_at_freq(n, x, fs, f)
        amps.append(amp)

    amps = np.array(amps)
    idx = np.argmax(amps)

    return freqs[idx], amps[idx]



print("\n===== Time-domain amplitude =====")
print("COM min:", np.nanmin(com_calc))
print("COM max:", np.nanmax(com_calc))
print("COM p2p:", np.nanmax(com_calc) - np.nanmin(com_calc))
print("COM peak_time:", (np.nanmax(com_calc) - np.nanmin(com_calc)) / 2)



f_best, A50_com = find_best_sine_freq(
    n, com, SAMPLE_RATE_HZ, 48.0, 52.0, 0.01
)

A50_diff, dc_diff = sine_fit_amp_at_freq(n, diff, SAMPLE_RATE_HZ, f_best)
A50_noise, dc_noise = sine_fit_amp_at_freq(n, noise_est, SAMPLE_RATE_HZ, f_best)
A50_clean, dc_clean = sine_fit_amp_at_freq(n, clean, SAMPLE_RATE_HZ, f_best)

print("\n===== sine-fit amplitude near 50Hz =====")
print(f"Best frequency from COM: {f_best:.2f} Hz")
print(f"COM   @ {f_best:.2f}Hz: {A50_com:.6f} mV peak")
print(f"DIFF  @ {f_best:.2f}Hz: {A50_diff:.6f} mV peak")
print(f"NOISE @ {f_best:.2f}Hz: {A50_noise:.6f} mV peak")
print(f"CLEAN @ {f_best:.2f}Hz: {A50_clean:.6f} mV peak")

if A50_clean > 0:
    print(f"\nANC improvement = {20*np.log10(A50_diff/A50_clean):.2f} dB")
    print(f"Effective rejection = {20*np.log10(A50_com/A50_clean):.2f} dB")


# =========================
# تحلیل پرش‌ها (سکته‌ها) با توجه به n
# =========================
dn = np.diff(n)
if len(dn) == 0:
    k_guess = None
else:
    vals, cnts = np.unique(dn, return_counts=True)
    order = np.argsort(cnts)[::-1]
    vals = vals[order]
    cnts = cnts[order]
    k_guess = int(vals[0]) if len(vals) else None

print("\nStep (dn) stats:")
if len(dn):
    print("  unique dn (first 10):", list(zip(vals[:10].tolist(), cnts[:10].tolist())))
print("  guessed expected step:", k_guess)

# سکته = dn != k_guess
if k_guess is None:
    jump_idx = np.array([], dtype=int)
else:
    jump_idx = np.where(dn != k_guess)[0]

print(f"\nJumps detected: {len(jump_idx)}")
for i in jump_idx[:20]:
    print(f"  jump at idx {i}: n {n[i]} -> {n[i+1]} (dn={dn[i]})")


# =========================
# (نقطه‌ی درستِ استفاده از فیلترها)
#   قبل از اینکه NaN تزریق کنیم،
#   روی سیگنال‌های خام فیلتر را اعمال می‌کنیم.
# =========================
com_f = com.copy()
diff_f = diff.copy()
noise_est_f = noise_est.copy()
clean_f = clean.copy()

if USE_NOTCH_50HZ:
    com_f = notch_filter(com_f, fs=SAMPLE_RATE_HZ, f0=NOTCH_F0_HZ, Q=NOTCH_Q)
    diff_f = notch_filter(diff_f, fs=SAMPLE_RATE_HZ, f0=NOTCH_F0_HZ, Q=NOTCH_Q)
    noise_est_f = notch_filter(noise_est_f, fs=SAMPLE_RATE_HZ, f0=NOTCH_F0_HZ, Q=NOTCH_Q)
    clean_f = notch_filter(clean_f, fs=SAMPLE_RATE_HZ, f0=NOTCH_F0_HZ, Q=NOTCH_Q)

if USE_LOWPASS:
    com_f = lowpass_filter(com_f, fs=SAMPLE_RATE_HZ, fc=LOWPASS_FC_HZ, Q=LOWPASS_Q)
    diff_f = lowpass_filter(diff_f, fs=SAMPLE_RATE_HZ, fc=LOWPASS_FC_HZ, Q=LOWPASS_Q)
    noise_est_f = lowpass_filter(noise_est_f, fs=SAMPLE_RATE_HZ, fc=LOWPASS_FC_HZ, Q=LOWPASS_Q)
    clean_f = lowpass_filter(clean_f, fs=SAMPLE_RATE_HZ, fc=LOWPASS_FC_HZ, Q=LOWPASS_Q)


# =========================
# قطع کردن خطوط در نمودار با NaN تا شکل خراب نشه
# (هم برای خام، هم برای فیلترشده)
# =========================
if len(jump_idx):
    # برای اینکه ایندکس‌ها بعد از insert به هم نریزه، از آخر به اول می‌ریم
    for idx in jump_idx[::-1]:
        n = np.insert(n, idx + 1, n[idx] + (k_guess if k_guess else 1))

        com = np.insert(com, idx + 1, np.nan)
        diff = np.insert(diff, idx + 1, np.nan)
        noise_est = np.insert(noise_est, idx + 1, np.nan)
        clean = np.insert(clean, idx + 1, np.nan)

        com_f = np.insert(com_f, idx + 1, np.nan)
        diff_f = np.insert(diff_f, idx + 1, np.nan)
        noise_est_f = np.insert(noise_est_f, idx + 1, np.nan)
        clean_f = np.insert(clean_f, idx + 1, np.nan)


# =========================
# رسم
# =========================
plt.figure(figsize=(12, 6))

# خام
# plt.plot(n, diff, label='DIFF raw (mV)', linewidth=1, alpha=0.35)
# plt.plot(n, com,  label='COM  raw (mV)', linewidth=1, alpha=0.35)
# plt.plot(n, clean,label='CLEAN raw (mV)', linewidth=1, alpha=0.35)

# فیلتر شده
plt.plot(n, diff_f, label='DIFF filtered (mV)', linewidth=1.3)
plt.plot(n, com_f, label='COM filtered (mV)', linewidth=1.3)
plt.plot(n, noise_est_f, label='NOISE_EST filtered (mV)', linewidth=1.2)
plt.plot(n, clean_f, label='CLEAN filtered (mV)', linewidth=1.3)

plt.xlabel('Sample index n (real ADC samples)')
plt.ylabel('mV')
plt.title(
    f'Window = {REAL_SAMPLES_WINDOW} real samples | expected step ~ {k_guess} | '
    f'fs={SAMPLE_RATE_HZ}Hz | notch={USE_NOTCH_50HZ} lp={USE_LOWPASS}'
)
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()
