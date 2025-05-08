import re
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation

ser = serial.Serial('COM3', 115200)  # پورت سریال خود را تنظیم کنید
data = []

def update(frame):
    line = ser.readline().decode('utf-8').strip()
    if line.isdigit():
        data.append(int(line))
        if len(data) > 1000000:  # نگه‌داری تنها 100 نمونه
            data.pop(0)
        ax.clear()
        ax.plot(data)
        ax.set_ylim(0, 4096)  # محدوده ADC برای 12 بیت

fig, ax = plt.subplots()
ani = animation.FuncAnimation(fig, update, interval=100)

plt.show()





# import serial
# import matplotlib.pyplot as plt
# import matplotlib.animation as animation

# ser = serial.Serial('COM3', 115200)  # تنظیم پورت سریال خود
# data = []

# fig, ax = plt.subplots()
# line, = ax.plot(data)
# ax.set_ylim(0, 4096)  # محدوده ADC برای 12 بیت

# def update(frame):
#     try:
#         line = ser.readline().decode('utf-8').strip()
#         if line.isdigit():
#             data.append(int(line))
#             if len(data) > 1000:  # نگه‌داری تنها 1000 نمونه
#                 data.pop(0)
#             line.set_ydata(data)
#             line.set_xdata(range(len(data)))
#             ax.set_xlim(0, len(data))  # تنظیم محور x بر اساس طول داده‌ها
#     except:
#         pass
#     return line,

# def init():
#     line.set_data([], [])
#     return line,

# ani = animation.FuncAnimation(fig, update, init_func=init, interval=50, blit=True)

# plt.show()
