import os
import re
import matplotlib.pyplot as plt

# ---------- تنظیم مسیر فایل ----------
# اگر این اسکریپت را کنار فولدر Putty اجرا می‌کنید، همین کافی است.
# در غیر این صورت، مسیر کامل فایل لاگ را اینجا بگذارید.
BASE_DIR = os.path.dirname(__file__) if "__file__" in globals() else os.getcwd()
# LOG_PATH = os.path.join(BASE_DIR, "Putty", "putty.log")
LOG_PATH = os.path.join(BASE_DIR, "putty.log")

# ---------- الگوهای لازم ----------
# حذف کدهای رنگ ANSI
ansi_re = re.compile(r"\x1b\[[0-9;]*m")

# استخراج زمان (ms)، sig و shifted_sig از هر خط
# مثال خط: I (116822) main: sig  = 0.301 | shifted_sig = -0.216
line_re = re.compile(
    r"I\s*\((\d+)\).*?COM=\s*([+-]?\d+(?:\.\d+)?)\s*,\s*DIFF=\s*([+-]?\d+(?:\.\d+)?)"
)


times_ms = []
sig_vals = []
shifted_vals = []

if not os.path.isfile(LOG_PATH):
    raise FileNotFoundError(f"فایل پیدا نشد: {LOG_PATH}")

with open(LOG_PATH, "r", encoding="utf-8", errors="ignore") as f:
    for raw_line in f:
        # پاک کردن کدهای رنگ
        line = ansi_re.sub("", raw_line)
        m = line_re.search(line)
        if m:
            t_ms = int(m.group(1))
            sig = float(m.group(2))
            shifted = float(m.group(3))
            times_ms.append(t_ms)
            sig_vals.append(sig)
            shifted_vals.append(shifted)

if not times_ms:
    raise ValueError("هیچ رکوردی با الگوی 'com' و 'diff' در لاگ پیدا نشد.")

# تبدیل زمان به ثانیه و صفر کردن نسبت به اولین نمونه
# t0 = times_ms[0]
# t_sec = [(t - t0) / 7812.5 for t in times_ms]

Fs = 7812.5  # Sample rate واقعی ADC
t_sec = [i / Fs for i in range(len(sig_vals))]



# ---------- رسم ----------
plt.figure(figsize=(12, 6))
plt.plot(t_sec, sig_vals, label="com")
plt.plot(t_sec, shifted_vals, label="diff")
plt.xlabel("زمان (ثانیه)")
plt.ylabel("دامنه سیگنال")
plt.title("نمایش همزمان com و diff بر حسب زمان")
plt.grid(True, which="both", linestyle=":", linewidth=0.7)
plt.legend()
plt.tight_layout()
# plt.savefig("signals.png", dpi=150)
plt.show()
