# kalman_1d

**Status: candidate**
**Layer: L0 (primitives)**

1D scalar Kalman filter for embedded sensor noise reduction.
Float-based predict/update equations; first-sample bootstrap; runtime
noise tuning; gain inspection.  Pure C99, no heap, fully reentrant
per-context.  Typical uses: ADC noise suppression, sensor signal
conditioning, state estimation on MCUs.

## Origin

Proactive build for embedded signal conditioning (ADR-011).

## Why self-written?

Searched GitHub (2026-05-06). All suitable Kalman libraries found were
either multi-dimensional (matrix-based, e.g. kalman-clib, TinyEKF) or
C++ template libraries.  No zero-dependency C99 scalar implementation
with platform-consistent error codes and test harness was found.
Self-written per ADR-010.

## Dependencies

None.  Pure C99 with `<string.h>` only; test builds link `-lm` for
`NAN` (test only, not required by the library itself).

## API

```c
/* Initialise.  cfg->q > 0 and cfg->r > 0 required. */
int   kalman_1d_init    (kalman_1d_t *kf, const kalman_1d_cfg_t *cfg);

/* Feed one measurement.  First call bootstraps: x=z, P=1, K=1.
   Returns updated state estimate, or 0.0f if kf is NULL. */
float kalman_1d_update  (kalman_1d_t *kf, float z);

/* Read current state estimate (NULL-safe, returns 0.0f). */
float kalman_1d_get     (const kalman_1d_t *kf);

/* Read last Kalman gain (NULL-safe, returns 0.0f). */
float kalman_1d_get_gain(const kalman_1d_t *kf);

/* Zero all fields.  Call init() again before reuse. */
void  kalman_1d_reset   (kalman_1d_t *kf);

/* Change Q and R at runtime.  Both must be > 0. */
int   kalman_1d_set_noise(kalman_1d_t *kf, float q, float r);
```

### Error codes

| Value | Meaning |
|-------|---------|
| `KALMAN_1D_OK` (0) | Success |
| `KALMAN_1D_ERR_PARAM` (-1) | NULL pointer or invalid parameter |

### Noise parameter guidance

| Parameter | Effect of increasing |
|-----------|---------------------|
| `q` (process noise) | Filter trusts measurements more; faster response to changes |
| `r` (measurement noise) | Filter smooths more; slower response, lower noise floor |

## Supported platforms

- [x] Host (sim) — passed 2026-05-06
- [ ] ARM Cortex-M — no hardware dependency expected

## Known issues

None.

## How to test

```bash
cd wheels/primitives/kalman_1d/test
cmake -GNinja -S . -B build && cmake --build build && ./build/test_kalman_1d
```

23 test cases covering: init (ok + null + invalid params + NaN guard on
both q and r), bootstrap first-sample behaviour, convergence to constant
value, noise rejection with deterministic noise, step response tracking,
gain decrease over time, Q/R parameter effect on response speed, reset,
set_noise (valid + invalid + NaN), NULL-safe accessors, and NaN
measurement rejection in update.

## Test status

- Unit tests (sim): PASS (23 tests, 0 failures)
- HIL: N/A

## API convention deviations

- Error codes are plain `#define` integers (`KALMAN_1D_OK`, `KALMAN_1D_ERR_PARAM`)
  rather than a `typedef enum`.  Consistent with the approach used in
  `iir_lowpass`, `bitfield`, and the crc family; both values fit on one line
  and a named enum would add no semantic value for a two-state return.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Bootstrap on p==0 rather than a separate flag keeps
  the struct minimal and avoids a bool field; NaN-safe `!(val > 0.0f)`
  guards reject NaN and negative values in a single expression; reset()
  zeroes the entire struct (including q/r) for simplicity — caller must
  re-init before reuse.

## Responsible human

@user
