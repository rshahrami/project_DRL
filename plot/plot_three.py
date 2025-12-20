import serial
import matplotlib.pyplot as plt

ser = serial.Serial('COM3', 921600)

com = []
diff = []

while True:
    line = ser.readline().decode().strip()
    # print(line)
    try:
        c, d = map(float, line.split(','))
        com.append(c)
        diff.append(d)
    except:
        continue

    if len(com) >= 5000:
        break

plt.plot(com)
plt.plot(diff)
plt.show()
