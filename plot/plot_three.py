
#########################################################################################

import serial
import matplotlib.pyplot as plt

PORT = 'COM3'
BAUD = 921600
NUM_SAMPLES = 1000

com = []
diff = []

print("Start")

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    ser.reset_input_buffer()

    while len(com) < NUM_SAMPLES:
        try:
            line = ser.readline().decode(errors='ignore').strip()
            if not line:
                continue

            c, d = map(float, line.split(','))
            com.append(c)
            diff.append(d)

        except ValueError:
            # خط‌هایی که فرمت ندارند
            continue

print("Stop")

plt.figure(figsize=(10, 5))
plt.plot(com, label='COM')
plt.plot(diff, label='DIFF')
plt.xlabel('نمونه‌ها')
plt.ylabel('ولتاژ (V)')
plt.title('Waveform از ESP32 (batch)')
plt.legend()
plt.grid(True)
plt.show()


######################################################################################
# import serial
# import struct
# import matplotlib.pyplot as plt

# PORT = 'COM3'
# BAUD = 921600
# NUM_SAMPLES = 1000  # تعداد نمونه‌هایی که می‌خواهی بخوانی

# com = []
# diff = []

# print("Start")

# with serial.Serial(PORT, BAUD, timeout=1) as ser:
#     ser.reset_input_buffer()

#     while len(com) < NUM_SAMPLES:
#         # هر نمونه دو int32 → 8 بایت
#         data = ser.read(8)
#         if len(data) < 8:
#             continue  # داده ناقص، دوباره بخوان

#         # تبدیل باینری به int32، little-endian
#         c, d = struct.unpack('<ii', data)

#         com.append(c)
#         diff.append(d)

# print("Stop")

# plt.figure(figsize=(10, 5))
# plt.plot(com, label='COM')
# plt.plot(diff, label='DIFF')
# plt.xlabel('نمونه‌ها')
# plt.ylabel('ADC raw value')
# plt.title('Waveform از ESP32 (binary stream)')
# plt.legend()
# plt.grid(True)
# plt.show()



