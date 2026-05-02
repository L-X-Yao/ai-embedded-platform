# lookup_table — interpolated lookup table for sensor linearization

**Status: candidate**
**Layer: L0 (primitives)**

Linearly interpolates between caller-provided (x, y) data points.
Designed for sensor linearization, calibration curves, and PWM duty-cycle
mapping. Out-of-range inputs are clamped to endpoint y values.

## Origin

Proactive build for sensor linearization and calibration curve use cases.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
/* Error codes */
LOOKUP_TABLE_OK            =  0
LOOKUP_TABLE_ERR_PARAM     = -1
LOOKUP_TABLE_ERR_NOT_SORTED = -2

/* Initialise with caller-owned sorted x/y arrays */
lookup_table_err_t lut_init(lookup_table_t *table,
                            const float *x, const float *y,
                            size_t count);

/* Interpolate y for a given x (clamps at endpoints) */
float lut_interpolate(const lookup_table_t *table, float x);
```

## Configuration (compile-time)

None. All behaviour is determined at runtime via `lut_init` parameters.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/lookup_table/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lookup_table
```

## Test status

- Unit tests (sim): 13 tests passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: clamp (not extrapolate) beyond endpoints; strictly ascending x required; binary search for O(log n) lookup
## Responsible human

@user
