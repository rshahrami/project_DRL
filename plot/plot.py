import serial
import re
import matplotlib.pyplot as plt

PORT = 'COM3'
BAUD = 921600
NUM_SAMPLES = 1000

# n,com,diff,clean  (n = uint32)
pat = re.compile(
    rb'^\s*(\d+),\s*([-+]?\d+(?:\.\d+)?),\s*([-+]?\d+(?:\.\d+)?),\s*([-+]?\d+(?:\.\d+)?)\s*$'
)

n_list = []
com_list = []
diff_list = []
clean_list = []

print("Start")

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    ser.reset_input_buffer()

    while len(clean_list) < NUM_SAMPLES:
        raw = ser.readline()
        m = pat.match(raw)
        if not m:
            continue

        n = int(m.group(1))
        c = float(m.group(2))
        d = float(m.group(3))
        e = float(m.group(4))

        n_list.append(n)
        com_list.append(c)
        diff_list.append(d)
        clean_list.append(e)

print("Stop")

# گزارش پرش‌ها
gaps = []
for i in range(1, len(n_list)):
    dn = n_list[i] - n_list[i-1]
    if dn != 1:
        gaps.append((i, n_list[i-1], n_list[i], dn))

print(f"Samples captured: {len(n_list)}")
if gaps:
    print(f"Found {len(gaps)} gap(s) in sample counter! Showing first 10:")
    for g in gaps[:10]:
        idx, prev_n, cur_n, dn = g
        print(f"  at index {idx}: n jumped {prev_n} -> {cur_n} (Δ={dn})")
else:
    print("No gaps detected in sample counter (perfect stream).")

# رسم با محور n (نه index)
plt.figure(figsize=(12, 5))
plt.plot(n_list, com_list, label='COM', linewidth=1)
plt.plot(n_list, diff_list, label='DIFF', linewidth=1)
plt.plot(n_list, clean_list, label='CLEAN_LP', linewidth=1)

plt.xlabel('Sample counter (n)')
plt.ylabel('mV')
plt.title('ESP32 stream (n, com, diff, clean_lp)')
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

#########################################################################################

# import serial
# import matplotlib.pyplot as plt

# PORT = 'COM3'
# BAUD = 921600
# NUM_SAMPLES = 20000

# com = []
# diff = []
# clean = []

# print("Start")

# with serial.Serial(PORT, BAUD, timeout=1) as ser:
#     ser.reset_input_buffer()

#     while len(clean) < NUM_SAMPLES:
#         try:
#             line = ser.readline().decode(errors='ignore').strip()
#             if not line:
#                 continue

#             c, d, e = map(float, line.split(','))
#             com.append(c)
#             # diff.append(d)
#             # com=0
#             clean.append(e)

#         except ValueError:
#             # خط‌هایی که فرمت ندارند
#             continue

# print("Stop")

# plt.figure(figsize=(10, 5))
# plt.plot(com, label='COM')
# # plt.plot(diff, label='DIFF')
# plt.plot(clean, label='CLEAN')
# plt.xlabel('نمونه‌ها')
# plt.ylabel('ولتاژ (V)')
# plt.title('Waveform از ESP32 (batch)')
# plt.legend()
# plt.grid(True)
# plt.show()




# #########################################################################################

# import serial
# import matplotlib.pyplot as plt

# PORT = 'COM3'
# BAUD = 921600
# NUM_SAMPLES = 4000

# com = []
# diff = []

# print("Start")

# with serial.Serial(PORT, BAUD, timeout=1) as ser:
#     ser.reset_input_buffer()

#     while len(com) < NUM_SAMPLES:
#         try:
#             line = ser.readline().decode(errors='ignore').strip()
#             if not line:
#                 continue

#             c, d = map(float, line.split(','))
#             com.append(c)
#             diff.append(d)

#         except ValueError:
#             # خط‌هایی که فرمت ندارند
#             continue

# print("Stop")

# plt.figure(figsize=(10, 5))
# plt.plot(com, label='COM')
# plt.plot(diff, label='DIFF')
# plt.xlabel('نمونه‌ها')
# plt.ylabel('ولتاژ (V)')
# plt.title('Waveform از ESP32 (batch)')
# plt.legend()
# plt.grid(True)
# plt.show()

