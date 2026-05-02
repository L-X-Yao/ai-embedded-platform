# mag_cal — min/max magnetometer calibration (hard-iron + scale)

**Status: candidate**
**Layer: L0 (primitives)**

Magnetometer calibration using min/max ellipsoid fitting. Computes hard-iron offset and soft-iron scale correction from raw 3-axis samples.
Zero hardware dependency — feed samples in, get calibration parameters out.

## Origin

Extracted from `projects/flybox/src/app/mag_cal.c` (QMC5883P magnetometer calibration).
Decoupled: FreeRTOS timing, flash persistence, LED indicators stay in the project;
pure min/max algorithm moves to this wheel.

## Why self-written?

Searched GitHub (2026-05-02). Referenced michal34512/Magnetometer-calibration (unclear license) for algorithm comparison. That library uses least-squares ellipsoid fitting; our simpler min/max hard-iron + scale approach is adequate for consumer-grade magnetometers. Self-written for platform convention consistency; algorithm cross-validated against reference.

## Dependencies

None. Pure C99 + libm.

## API

```c
mag_cal_t cal;
mag_cal_init(&cal);
mag_cal_start(&cal);

/* Feed samples from magnetometer (e.g., 10-50 Hz for ~10-30 seconds) */
while (collecting) {
    mag_cal_feed(&cal, raw_mx, raw_my, raw_mz);
}

if (mag_cal_compute(&cal) == MAG_CAL_DONE_OK) {
    const mag_cal_result_t *r = mag_cal_get_result(&cal);
    /* Save r->offset_x/y/z and r->scale_x/y/z to flash */

    /* Apply to future readings */
    float cx, cy, cz;
    mag_cal_apply(r, raw_mx, raw_my, raw_mz, &cx, &cy, &cz);
}
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `MAG_CAL_MAX_SAMPLES` | 500 | Maximum sample buffer size |
| `MAG_CAL_MIN_SPREAD` | 0.005 | Minimum radius per axis to accept calibration |

## Supported platforms

- [x] Host (sim) — passed 2026-04-28
- [ ] GD32F405 (flybox) — original project code verified, wheel not separately HIL tested

## Known issues

- Static buffer of `MAG_CAL_MAX_SAMPLES * 3 * 4` bytes (6 KB at default 500). Reduce on RAM-constrained MCUs.
- Min/max method assumes user rotates sensor to cover all orientations. Poor coverage produces inaccurate results.
- No outlier rejection — a single extreme sample skews min/max. Collect in magnetically clean environment.
- Scale factors assume ellipsoidal distortion; more complex soft-iron (rotation) is not corrected.

## How to test

```bash
cd wheels/primitives/mag_cal/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mag_cal
```

15 test cases: init, start, feed-before-start rejected, perfect sphere calibration, ellipsoid calibration with scale correction, apply (center→zero), insufficient spread failure, zero samples failure, null safety, re-init clears state, buffer full boundary, single sample, collinear samples, apply null outputs, compute before start.

## Test status

- Unit tests (sim): PASS (15/15 C tests)
- HIL: N/A

## API convention deviations

- `mag_cal_t` contains a large sample buffer (6 KB). Caller must ensure sufficient stack/heap. This is inherent to the min/max algorithm needing all samples in memory.

## API review

- **Date**: 2026-04-28
- **Key decisions**: Stateful collection (feed → compute two-phase) decouples timing from algorithm; result struct by pointer for caller-managed persistence; apply() is pure and separate from compute() for use after loading saved calibration; no RTOS dependency (caller manages timing/scheduling).

## Responsible human

@user

## Algorithm

1. Collect N samples while user rotates sensor in all orientations
2. Find min/max per axis → compute center (hard-iron offset) and radius
3. Normalize radii to average → compute scale factors (soft-iron correction)
4. Validate spread ≥ threshold per axis (reject stationary data)
