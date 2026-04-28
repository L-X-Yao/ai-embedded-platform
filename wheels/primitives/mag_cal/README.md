# mag_cal — min/max magnetometer calibration (hard-iron + scale)

**Status: candidate**
**Layer: L0 (primitives)**

Magnetometer calibration using min/max ellipsoid fitting. Computes hard-iron offset and soft-iron scale correction from raw 3-axis samples.
Zero hardware dependency — feed samples in, get calibration parameters out.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`calibrat`, `mag_cal`, `hard_iron`, `ellipsoid` — zero results.

VSF has no sensor calibration capability.

## Origin

Extracted from `projects/flybox/src/app/mag_cal.c` (QMC5883P magnetometer calibration).
Decoupled: FreeRTOS timing, flash persistence, LED indicators stay in the project;
pure min/max algorithm moves to this wheel.

## Algorithm

1. Collect N samples while user rotates sensor in all orientations
2. Find min/max per axis → compute center (hard-iron offset) and radius
3. Normalize radii to average → compute scale factors (soft-iron correction)
4. Validate spread ≥ threshold per axis (reject stationary data)

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

## Dependencies

None. Pure C99 + libm.

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

## Tests

```bash
cd wheels/primitives/mag_cal/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mag_cal
```

531 assertions across 11 test cases: init, start, feed-before-start rejected, perfect sphere calibration, ellipsoid calibration with scale correction, apply (center→zero), insufficient spread failure, zero samples failure, null safety, re-init clears state, buffer full boundary.

## Test status

- Unit tests (sim): PASS (11 C tests, 531 assertions)
- HIL: N/A

## Convention Deviations

- `mag_cal_t` contains a large sample buffer (6 KB). Caller must ensure sufficient stack/heap. This is inherent to the min/max algorithm needing all samples in memory.

## API review

- **Date**: 2026-04-28
- **Key decisions**: Stateful collection (feed → compute two-phase) decouples timing from algorithm; result struct by pointer for caller-managed persistence; apply() is pure and separate from compute() for use after loading saved calibration; no RTOS dependency (caller manages timing/scheduling).
