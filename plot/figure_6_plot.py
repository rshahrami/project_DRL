import numpy as np
import matplotlib.pyplot as plt

# =========================
# Data
# =========================
data = {
    # 0.05:   [1.32],
    0.1:    [2.34],
    0.2:    [3.44],
    0.70:   [4.30, 4.29, 4.25, 4.19, 4.319],
    0.97:   [4.4, 4.34, 4.41, 4.24],
    2.17:   [4.4, 4.38, 4.39],
    9.80:   [4.39, 4.46, 4.47],
    19.23:  [4.43, 4.415, 4.42],
    47.10:  [4.4, 4.37, 4.42],
    67.56:  [4.32, 4.23, 4.25],
    86.20:  [4.23, 4.2, 4.13, 4.19],
    106.63: [4.20, 4.00, 4.03],
    129.57: [3.8, 3.87],
    158.73: [3.6, 3.65],
    199.83: [3.2],
}

freqs = np.array(list(data.keys()))
means = np.array([np.mean(v) for v in data.values()])

# Reference = maximum measured mean amplitude
ref_amp = np.max(means)
ref_freq = freqs[np.argmax(means)]

amp_db = 20 * np.log10(means / ref_amp)

print("Frequency | Mean output | Relative dB")
for f, m, db in zip(freqs, means, amp_db):
    print(f"{f:7.2f} Hz | {m:8.4f} mV | {db:7.3f} dB")

print(f"\nReference: {ref_freq:.2f} Hz, {ref_amp:.4f} mV")

# =========================
# Annotation control
# True = show arrow
# False = hide arrow
# =========================
show_labels = {
    # 0.05: True,
    0.1: False,
    0.2: True,
    0.70: True,
    0.97: False,
    2.17: False,
    9.80: True,
    19.23: False,
    47.10: True,
    67.56: False,
    86.20: False,
    106.63: False,
    129.57: False,
    158.73: False,
    199.83: True,
}

# Optional custom offsets for arrow labels
offsets = {
    0.1:    (20, 25),
    0.2:    (20, 15),
    0.70:   (5, -35),
    0.97:   (20, 25),
    2.17:   (20, 25),
    9.80:   (20, 25),
    19.23:  (20, 25),
    47.10:  (-70, -30),
    67.56:  (20, -35),
    86.20:  (20, -35),
    106.63: (20, -35),
    199.83: (-110, -30),
}

# =========================
# Plot
# =========================
plt.figure(figsize=(7, 4.5))

plt.semilogx(
    freqs,
    amp_db,
    'o-',
    linewidth=1.8,
    markersize=6,
    color='red',
    markerfacecolor='red',
)

plt.axhline(0, color='black', linestyle='--', linewidth=0.8)
plt.axhline(-3, color='gray', linestyle='--', linewidth=0.8)

plt.xlabel('Frequency (Hz)')
plt.ylabel('Amplitude (dB)')
# plt.title('Frequency Response Measurement Result')

plt.grid(True, which='both', linestyle='--', linewidth=0.5, alpha=0.7)

plt.xlim(0.1, 200)
plt.ylim(-6, 1)

# =========================
# Arrows / labels
# =========================
for f, db in zip(freqs, amp_db):
    if show_labels.get(float(f), False):
        text = f"({f:.2f} Hz, {db:.2f} dB)"
        dx, dy = offsets.get(float(f), (20, 20))

        plt.annotate(
            text,
            xy=(f, db),
            xytext=(dx, dy),
            textcoords='offset points',
            arrowprops=dict(arrowstyle='->', linewidth=0.9),
            fontsize=9,
        )

plt.tight_layout()
plt.savefig("figure6_frequency_response.png", dpi=600)
plt.savefig("figure6_frequency_response.pdf")
plt.show()