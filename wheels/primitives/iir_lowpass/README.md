# iir_lowpass

**Status: candidate**
**Layer: L0 (primitives)**

First-order IIR low-pass filter (exponential moving average / single-pole lag).
Pure C99, no heap, fully reentrant per-instance.

Formula: `Y[n] = alpha * X[n] + (1 - alpha) * Y[n-1]`

- **alpha → 1**: fast response, less smoothing (tracks input closely)
- **alpha → 0**: slow response, heavy smoothing (rejects high-frequency noise)

Typical uses: ADC noise reduction, temperature signal conditioning, sensor
pre-filtering before PID or AHRS algorithms.

## Origin

Proactive build (ADR-011). First-order IIR is the single most common signal
filter in embedded systems — appears in virtually every ADC or sensor pipeline.

## Why self-written?

Searched GitHub (2026-05-06). First-order IIR is a trivial formula with no
standalone library worth vendoring. Self-written for platform convention
consistency (unified error codes, NaN-safe guards, test harness integration).

## Dependencies

None. Pure C99 with `<stdint.h>`.

## API

```c
int   iir_lowpass_init(iir_lowpass_t *f, const iir_lowpass_cfg_t *cfg);
float iir_lowpass_feed(iir_lowpass_t *f, float x);
float iir_lowpass_get(const iir_lowpass_t *f);
void  iir_lowpass_reset(iir_lowpass_t *f);
int   iir_lowpass_set_alpha(iir_lowpass_t *f, float alpha);
float iir_lowpass_alpha_from_rc(float dt, float rc);
```

- `init`: `alpha` must be in `(0, 1]`; NaN-safe guard `!(alpha > 0 && alpha <= 1)`.
- `feed`: first sample sets output directly (no startup lag). Returns `0.0f` on NULL.
- `reset`: clears output and primed flag, keeps alpha.
- `set_alpha`: change cutoff at runtime without losing filter state.
- `alpha_from_rc`: `alpha = dt / (rc + dt)` — converts RC time constant to alpha.

## Configuration (compile-time)

None. Alpha is a runtime parameter.

## Supported platforms

- [x] Host (sim) — passed 2026-05-06
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

- `iir_lowpass_feed`: NaN input is not guarded. If the sample `x` is NaN,
  the output becomes NaN and remains NaN until `iir_lowpass_reset()` is
  called. This is intentional — filtering NaN requires knowledge of the
  "correct" substitute value, which is application-specific. Callers must
  sanitize upstream sensor faults before feeding.

## How to test

```bash
cd wheels/primitives/iir_lowpass/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_iir_lowpass
./build/test_null_iir_lowpass
```

20 test cases: init validation (NULL ptr, alpha range, NaN), first-sample
passthrough, step-response smoothing, alpha=1 full passthrough, small-alpha
slow response, 100-sample convergence, reset, runtime alpha change, NaN
rejection in set_alpha, NaN feed propagation and reset recovery, NULL guards
on all pointer-taking functions, RC-to-alpha helper (including NaN inputs).

6 null-parameter tests auto-generated for all pointer-taking public API
functions.

## Test status

- Unit tests (sim): PASS (20 C tests + 6 null tests)
- HIL: N/A

## API convention deviations

- Error codes are plain `#define` integers (`IIR_LOWPASS_OK`, `IIR_LOWPASS_ERR_PARAM`)
  rather than a typedef enum. Consistent with the approach used in crc8/crc16/crc32.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Single filter type (no SMA); `cfg` struct for extensibility
  without breaking API changes; first sample sets output directly to avoid
  startup transients; alpha=0 rejected (would ignore all input); NaN-safe guard
  pattern `!(alpha > 0 && alpha <= 1)` catches NaN and avoids implicit
  NaN-comparisons.

## Responsible human
@L-X-Yao
