
#########################################################################################

import serial
import matplotlib.pyplot as plt

PORT = 'COM3'
BAUD = 921600
NUM_SAMPLES = 4000

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

