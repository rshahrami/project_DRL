import re
import matplotlib.pyplot as plt

timestamps_ms = []
adc_values = []

# الگو را اینجا تعریف کنید
regex_pattern = r"\((\d+)\)\s*\w+:\s*(\-?\d+\.\d+)" # یا هر کلمه‌ای بعد از پرانتز، مثلا "main:"

try:
    with open('your_log_file.txt', 'r') as f: # نام فایل لاگ خود را اینجا قرار دهید
        for line in f:
            match = re.search(regex_pattern, line)
            if match:
                try:
                    timestamps_ms.append(int(match.group(1)))
                    adc_values.append(float(match.group(2)))
                except ValueError:
                    print(f"خطا در تبدیل داده در خط: {line.strip()}")
                    # می‌توانید تصمیم بگیرید که آیا می‌خواهید در این حالت ادامه دهید یا متوقف شوید

except FileNotFoundError:
    print("خطا: فایل لاگ پیدا نشد.")
except Exception as e:
    print(f"یک خطا در هنگام خواندن یا پردازش فایل رخ داده است: {e}")

# --- تبدیل زمان به ثانیه و نرمال‌سازی ---
if timestamps_ms:
    first_timestamp_ms = timestamps_ms[0]
    timestamps_sec_normalized = [(t - first_timestamp_ms) / 1000.0 for t in timestamps_ms]
else:
    timestamps_sec_normalized = []


# --- بخش رسم نمودار ---
if adc_values and timestamps_sec_normalized and len(adc_values) == len(timestamps_sec_normalized):
    plt.figure(figsize=(12, 6))
    plt.plot(timestamps_sec_normalized, adc_values, marker='.', linestyle='-', color='purple', markersize=3) # تغییر رنگ و نشانگر

    plt.title('مقادیر ADC از ESP32 بر حسب زمان')
    plt.xlabel('زمان سپری شده از اولین نمونه (ثانیه)')
    plt.ylabel('مقدار ولتاژ ADC (یا مقدار خام)') # برچسب محور Y را دقیق‌تر کنید
    plt.grid(True, which='both', linestyle=':', linewidth=0.7) # تغییر سبک شبکه
    plt.tight_layout()
    plt.show()
else:
    if not adc_values or not timestamps_sec_normalized:
        print("داده کافی برای رسم نمودار (زمان یا مقادیر ADC) استخراج نشده است.")
    elif len(adc_values) != len(timestamps_sec_normalized):
        print("تعداد زمان‌های استخراج شده با تعداد مقادیر ADC برابر نیست. نمودار رسم نمی‌شود.")



sampling_interval_seconds = 0.01 # برای مثال، 10 میلی‌ثانیه
timestamps_sec_generated = [i * sampling_interval_seconds for i in range(len(adc_values))]
# سپس از timestamps_sec_generated برای محور X استفاده کنید.
