import os
import re
import matplotlib.pyplot as plt

x = []
init_step = 0
step_timer = 0.0001

current_path = os.path.dirname(__file__)
file = os.path.join(current_path, "Putty\putty.log")

extracted_numbers = []

# الگوی عبارت باقاعده اصلاح شده برای پیدا کردن اعداد اعشاری (مثبت و منفی)
# \-?  علامت منفی اختیاری در ابتدای عدد
# \d+  یک یا چند رقم
# \.   کاراکتر نقطه
# \d+  یک یا چند رقم
regex_pattern = r"(\-?\d+\.\d+)"

for line in open(file):
    match = re.search(regex_pattern, line)
    if match:
        extracted_numbers.append(float(match.group(1)))


sampling_interval_seconds = 0.02 # برای مثال، 10 میلی‌ثانیه
timestamps_sec_generated = [i * sampling_interval_seconds for i in range(len(extracted_numbers))]
# سپس از timestamps_sec_generated برای محور X استفاده کنید.

# --- بخش رسم نمودار ---
if extracted_numbers and timestamps_sec_generated and len(extracted_numbers) == len(timestamps_sec_generated):
    plt.figure(figsize=(12, 6))
    plt.plot(timestamps_sec_generated, extracted_numbers, marker='.', linestyle='-', color='purple', markersize=3) # تغییر رنگ و نشانگر

    # plt.title('مقادیر ADC از ESP32 بر حسب زمان')
    # plt.xlabel('زمان سپری شده از اولین نمونه (ثانیه)')
    # plt.ylabel('مقدار ولتاژ ADC (یا مقدار خام)') # برچسب محور Y را دقیق‌تر کنید
    plt.grid(True, which='both', linestyle=':', linewidth=0.7) # تغییر سبک شبکه
    plt.tight_layout()
    plt.show()
else:
    if not extracted_numbers or not timestamps_sec_generated:
        print("داده کافی برای رسم نمودار (زمان یا مقادیر ADC) استخراج نشده است.")
    elif len(extracted_numbers) != len(timestamps_sec_generated):
        print("تعداد زمان‌های استخراج شده با تعداد مقادیر ADC برابر نیست. نمودار رسم نمی‌شود.")

