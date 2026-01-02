import serial
import re
import numpy as np
import matplotlib.pyplot as plt

PORT = 'COM3'
BAUD = 921600

# --- تنظیم پنجره ---
REAL_SAMPLES_WINDOW = 3000   # سه سیکل 1Hz در 1kSPS
WARMUP_SKIP = 0             # اگر ESP خودش warmup را چاپ نمی‌کند، اینجا می‌توانی n اولیه را نادیده بگیری

# اگر خروجی تو float mV است:
MODE = "float_mV"
# اگر خروجی را به int (mV*1000) تبدیل کردی، این را بگذار:
# MODE = "int_uV_in_mVx1000"

# حذف کدهای رنگ/ANSI (اگر PuTTY/monitor چیزی اضافه کند)
ansi = re.compile(rb'\x1b\[[0-9;]*m')

# الگوی داده: n,com,diff,clean
pat_float = re.compile(
    rb'^\s*(\d+),\s*([-+]?\d+(?:\.\d+)?),\s*([-+]?\d+(?:\.\d+)?),\s*([-+]?\d+(?:\.\d+)?)\s*$'
)
# الگوی داده‌ی int (mV*1000): n,com_i,diff_i,clean_i
pat_int = re.compile(
    rb'^\s*(\d+),\s*([-+]?\d+),\s*([-+]?\d+),\s*([-+]?\d+)\s*$'
)

# توکن‌های لاگ/خطا که باید کامل ignore شوند
bad_tokens = (
    b'task_wdt', b'watchdog', b'guru meditation', b'backtrace',
    b'assert', b'abort', b'failed',
    b'E (', b'W (', b'I (', b'D ('
)

def is_log_line(b: bytes) -> bool:
    low = b.lower()
    return any(tok in low for tok in bad_tokens)

def parse_data_line(b: bytes):
    """return (n, com, diff, clean) in mV as float, or None"""
    if MODE == "float_mV":
        m = pat_float.match(b)
        if not m:
            return None
        n = int(m.group(1))
        com = float(m.group(2))
        diff = float(m.group(3))
        clean = float(m.group(4))
        return n, com, diff, clean

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

# --- بافرهای داده ---
n_list = []
com_list = []
diff_list = []
clean_list = []

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

        n, com, diff, clean = parsed

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
        clean_list.append(clean)

        # شرط پایان: پنجره‌ی 3000 نمونه‌ی واقعی
        if (n - n_start) >= REAL_SAMPLES_WINDOW:
            break

print("Stop.")
print(f"Captured printed lines: {len(n_list)}")
print(f"Real samples covered:  {n_list[-1] - n_start}")
print(f"Skipped log/error lines: {skipped_logs}")
print(f"Skipped other malformed lines: {skipped_other}")

# --- تحلیل پرش‌ها (سکته‌ها) ---
n = np.array(n_list, dtype=np.int64)
com = np.array(com_list, dtype=float)
diff = np.array(diff_list, dtype=float)
clean = np.array(clean_list, dtype=float)

dn = np.diff(n)

# حدس K از شایع‌ترین dn
vals, cnts = np.unique(dn, return_counts=True)
k_guess = int(vals[np.argmax(cnts)]) if len(vals) else None

print("\nStep (dn) stats:")
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

# --- قطع کردن خطوط در نمودار با NaN تا شکل خراب نشه ---
# هر جا jump داریم، یک NaN بعدش تزریق می‌کنیم
for idx in jump_idx[::-1]:
    n = np.insert(n, idx + 1, n[idx] + 1)
    com = np.insert(com, idx + 1, np.nan)
    diff = np.insert(diff, idx + 1, np.nan)
    clean = np.insert(clean, idx + 1, np.nan)

# --- رسم ---
plt.figure(figsize=(12, 5))
plt.plot(n, clean, label='CLEAN (mV)', linewidth=1)
plt.plot(n, diff, label='DIFF (mV)', linewidth=1, alpha=0.5)
plt.plot(n, com, label='COM (mV)', linewidth=1, alpha=0.5)

plt.xlabel('Sample index n (real ADC samples)')
plt.ylabel('mV')
plt.title(f'Window = {REAL_SAMPLES_WINDOW} real samples | expected step ~ {k_guess}')
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()
