// SPDX-License-Identifier: MIT
/* iir_lowpass — First-order IIR low-pass filter
 * Layer: L0  Status: candidate  Deps: (none)  Instances: multi
 *
 * Formula: Y[n] = alpha * X[n] + (1-alpha) * Y[n-1]
 *
 * alpha close to 1 → fast response (less smoothing)
 * alpha close to 0 → slow response (heavy smoothing)
 *
 * Pure C99, no heap, fully reentrant per-instance.
 * Thread safety: not thread-safe. Callers must serialize access.
 */
#ifndef IIR_LOWPASS_H
#define IIR_LOWPASS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IIR_LOWPASS_OK         =  0,
    IIR_LOWPASS_ERR_PARAM  = -1,
} iir_lowpass_err_t;

typedef struct {
    float alpha;
} iir_lowpass_cfg_t;

typedef struct {
    float   alpha;
    float   output;
    uint8_t primed;  /* 0 until first sample has been fed */
} iir_lowpass_t;

/* Initialize filter. alpha must be in (0, 1].
 * @thread_safe: no  @isr_safe: no */
iir_lowpass_err_t iir_lowpass_init(iir_lowpass_t *f, const iir_lowpass_cfg_t *cfg);

/* Feed a new sample, return filtered output.
 * First sample sets output directly (no lag on startup).
 * Returns 0.0f on NULL pointer.
 * NaN input is not guarded: if x is NaN the output becomes NaN and
 * remains NaN until iir_lowpass_reset() is called. Callers must
 * sanitize upstream sensor faults before feeding.
 * @thread_safe: no  @isr_safe: yes (if instance not shared) */
float iir_lowpass_feed(iir_lowpass_t *f, float x);

/* Get the current filter output without feeding a new sample.
 * Returns 0.0f on NULL pointer.
 * @thread_safe: yes  @isr_safe: yes */
float iir_lowpass_get(const iir_lowpass_t *f);

/* Reset state: clear output and primed flag, keep alpha.
 * @thread_safe: no  @isr_safe: no */
void iir_lowpass_reset(iir_lowpass_t *f);

/* Change alpha at runtime. alpha must be in (0, 1].
 * @thread_safe: no  @isr_safe: no */
iir_lowpass_err_t iir_lowpass_set_alpha(iir_lowpass_t *f, float alpha);

/* Compute alpha from RC time constant: alpha = dt / (rc + dt).
 * dt = sample period (s), rc = time constant (s).
 * Both must be > 0. Returns 0.0f on invalid input.
 * @thread_safe: yes (pure function, no state)  @isr_safe: yes */
float iir_lowpass_alpha_from_rc(float dt, float rc);

#ifdef __cplusplus
}
#endif
#endif /* IIR_LOWPASS_H */
