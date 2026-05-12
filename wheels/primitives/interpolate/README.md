# interpolate -- linear, table, and bilinear interpolation

**Status: candidate**
**Layer: L0 (primitives)**

Linear interpolation, piecewise-linear lookup table interpolation (binary search),
and bilinear 2D grid interpolation. Designed for sensor calibration curves and
temperature-compensated lookup tables. All functions are pure, reentrant, and
use no heap.

## Origin

Proactive build for sensor calibration. Common need across NTC thermistors,
pressure sensors, ADC linearization, and temperature-compensated measurements.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- Most interpolation libraries are part of larger math/DSP frameworks.
- No standalone zero-dependency C99 interpolation library with NaN guards found.

Reference: None.

## Dependencies

None. Uses `<math.h>` only implicitly via float operations (no `libm` call in source).

## API

```c
#include "interpolate.h"

interpolate_err_t interpolate_linear(float x0, float y0, float x1, float y1,
                                     float x, float *out);
interpolate_err_t interpolate_table(const interpolate_table_t *table,
                                    float x, float *out);
interpolate_err_t interpolate_bilinear(float x0, float y0, float x1, float y1,
                                       float q00, float q10, float q01, float q11,
                                       float x, float y, float *out);
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim) -- 2026-05-12
- [ ] ARM Cortex-M -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/interpolate/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_interpolate
```

## Test status

- Unit tests (sim): PASS (13 cases)
- HIL: N/A (pure compute)

## API convention deviations

None. No `_init` function needed; all functions are stateless.

## API review

- **Date**: 2026-05-12
- **Key decisions**:
  - Table interpolation clamps at boundaries (no extrapolation)
  - Binary search for bracket finding (O(log n))
  - NaN guard on all inputs and outputs
  - Sorted-ascending validation on table x values

## Responsible human

@user
