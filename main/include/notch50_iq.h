#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float fs;      // sample rate (Hz)
    float f0;      // target notch frequency (Hz), typically 50
    float alpha;   // IIR smoothing factor (0..1). Smaller => slower but smoother.
    float theta;   // internal oscillator phase [0..2pi)

    // IQ states for com and diff
    float Ic, Qc;
    float Id, Qd;
} Notch50IQ;

/**
 * @brief Initialize the notch module.
 * @param st    pointer to state
 * @param fs    sample rate in Hz (e.g., 1000)
 * @param f0    notch frequency in Hz (e.g., 50)
 * @param alpha smoothing factor (e.g., 0.005 for ~200ms @1kHz)
 */
void notch50iq_init(Notch50IQ* st, float fs, float f0, float alpha);

/**
 * @brief Reset internal state (IQ accumulators and phase).
 */
void notch50iq_reset(Notch50IQ* st);

/**
 * @brief Process one sample pair (com, diff) and return cleaned diff.
 * @return cleaned diff sample
 */
float notch50iq_process(Notch50IQ* st, float com, float diff);

/**
 * @brief Optional: get latest estimated amplitudes/phases (debug/telemetry).
 * A ≈ amplitude of 50Hz component.
 * phi ≈ phase of 50Hz component (radians).
 */
void notch50iq_get_estimates(const Notch50IQ* st,
                            float* Ac, float* phic,
                            float* Ad, float* phid);

#ifdef __cplusplus
}
#endif
