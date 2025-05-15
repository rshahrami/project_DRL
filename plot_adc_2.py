import os
import re
import matplotlib.pyplot as plt

x = []
init_step = 0
step_timer = 0.0001

current_path = os.path.dirname(__file__)
file = os.path.join(current_path, "Putty\putty.log")

# ansi_escape =re.compile(r'(\x9B|\x1B\[)[0-?]*[ -\/]*[@-~]')
# ansi_escape =re.compile(r'\x1b[0;32mI')



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

# # چاپ اعداد استخراج شده
# for number in extracted_numbers:
#     print(number)


# --- بخش رسم نمودار ---
if extracted_numbers:  # بررسی اینکه آیا داده ای برای رسم وجود دارد
    # ایجاد یک دنباله از اعداد برای محور x (به تعداد داده های استخراج شده)
    # این نشان دهنده ترتیب یا "زمان" نمونه ها است
    x_values = range(len(extracted_numbers))

    # plt.figure(figsize=(10, 6))  # تنظیم اندازه نمودار (اختیاری)
    plt.plot(x_values, extracted_numbers, marker='o', linestyle='-', color='b') # رسم داده ها

    # اضافه کردن عنوان و برچسب به محورها
    plt.title('نمودار داده های استخراج شده از لاگ')
    plt.xlabel('ترتیب نمونه / زمان (نمونه شماره)')
    plt.ylabel('مقدار عددی')

    # اضافه کردن شبکه به نمودار (اختیاری)
    plt.grid(True)

    # نمایش نمودار
    plt.show()
else:
    print("هیچ داده ای برای رسم پیدا نشد.")
