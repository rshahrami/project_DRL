import serial
import re
import numpy as np
import matplotlib.pyplot as plt
from scipy.signal import welch

# =========================
# Serial settings
# =========================
PORT = 'COM3'
BAUD = 921600
SAMPLE_RATE_HZ = 1000.0

RECORD_SECONDS = 30
N_SAMPLES = int(RECORD_SECONDS * SAMPLE_RATE_HZ)

# اگر گین تفاضلی مدار 1 است، همین را 1 بگذار
GDIFF = 1.0

# =========================
# Regex: n,com,diff
# =========================
pat_float = re.compile(
    rb'^\s*(\d+)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*,\s*([-+]?\d+(?:\.\d+)?)\s*$'
)

ansi = re.compile(rb'\x1b\[[0-9;]*[A-Za-z]')

def parse_line(b):
    b = ansi.sub(b'', b).strip()
    m = pat_float.match(b)
    if not m:
        return None
    n = int(m.group(1))
    com = float(m.group(2))
    diff = float(m.group(3))
    return n, com, diff

# =========================
# Read data
# =========================
n_list = []
diff_list = []

print("Start noise recording...")

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    ser.reset_input_buffer()

    while len(diff_list) < N_SAMPLES:
        raw = ser.readline()
        if not raw:
            continue

        parsed = parse_line(raw)
        if parsed is None:
            continue

        n, com, diff = parsed
        n_list.append(n)
        diff_list.append(diff)

print("Stop.")
print(f"Captured samples: {len(diff_list)}")

n = np.array(n_list, dtype=np.int64)
diff = np.array(diff_list, dtype=float)  # mV

# =========================
# Use longest continuous segment
# =========================
def longest_continuous_segment(n, x, expected_step=1):
    dn = np.diff(n)
    if len(dn) == 0:
        return n, x

    segments = []
    start = 0

    for i, d in enumerate(dn):
        if d != expected_step:
            segments.append((start, i + 1))
            start = i + 1

    segments.append((start, len(n)))
    a, b = max(segments, key=lambda s: s[1] - s[0])

    print(f"Continuous segment: {b-a} samples")
    print(f"Segment n: {n[a]} -> {n[b-1]}")

    return n[a:b], x[a:b]

n_use, x_mV = longest_continuous_segment(n, diff)

# =========================
# Remove DC
# =========================
x_mV = x_mV[np.isfinite(x_mV)]
x_mV = x_mV - np.mean(x_mV)

# Convert output-referred to input-referred
x_in_mV = x_mV / GDIFF

# =========================
# Welch PSD
# =========================
nperseg = min(4096, len(x_in_mV))
noverlap = nperseg // 2

f, psd_mV2_Hz = welch(
    x_in_mV,
    fs=SAMPLE_RATE_HZ,
    window='hann',
    nperseg=nperseg,
    noverlap=noverlap,
    scaling='density'
)

# mV/sqrt(Hz) -> uV/sqrt(Hz)
noise_density_uV = np.sqrt(psd_mV2_Hz) * 1000.0

# =========================
# Integrated RMS noise
# =========================
band = (f >= 0.1) & (f <= 70.0)

noise_rms_mV = np.sqrt(np.trapezoid(psd_mV2_Hz[band], f[band]))
noise_rms_uV = noise_rms_mV * 1000.0

print(f"Input-referred noise 0.1–70 Hz = {noise_rms_uV:.3f} uVrms")

# =========================
# Plot Figure 7
# =========================
plt.figure(figsize=(6, 4.2))

plt.loglog(f, noise_density_uV, color='black', linewidth=1.2)

plt.xlabel('Frequency (Hz)')
plt.ylabel('Input-referred voltage noise (µV/√Hz)')
plt.title('Input-referred Voltage Noise Density')

plt.xlim(0.1, 100)
plt.ylim(1e-3, max(noise_density_uV[(f >= 0.1) & (f <= 100)]) * 2)

plt.grid(True, which='both', linestyle='--', linewidth=0.5, alpha=0.7)

plt.text(
    0.12,
    max(noise_density_uV[(f >= 0.1) & (f <= 100)]) * 0.8,
    f'0.1–70 Hz noise = {noise_rms_uV:.2f} µVrms',
    fontsize=9
)
print("mean =", np.mean(diff))
print("std  =", np.std(diff))
print("p2p  =", np.max(diff)-np.min(diff))
plt.tight_layout()
plt.savefig('figure7_input_referred_noise_density.png', dpi=600)
plt.savefig('figure7_input_referred_noise_density.pdf')
plt.show()