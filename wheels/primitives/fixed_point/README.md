# fixed_point — Q16.16 fixed-point math library

**Status: candidate**
**Layer: L0 (primitives)**

Overflow-safe Q16.16 fixed-point arithmetic for MCUs without FPU.
Signed 32-bit format: 16 bits integer + 16 bits fraction. Range
-32768.0 to +32767.99998, resolution ~0.0000153. All functions are
pure (no side effects, no global state, no malloc).

## Origin

Proactive build for embedded math on Cortex-M0/M3 and other cores
without hardware floating-point.

## Why self-written?

Searched GitHub (2026-05-02). Referenced PetteriAimonen/libfixmath (600 stars, MIT) for algorithm verification. libfixmath is the canonical Q16.16 library; our subset covers platform needs with int64_t overflow-safe intermediates. Self-written for platform convention consistency; algorithm cross-validated against reference.

## Dependencies

None.

## API

```c
typedef int32_t fp_t;   /* Q16.16 fixed-point */

/* Conversion */
fp_t    fp_from_int(int16_t val);
fp_t    fp_from_float(float val);
int16_t fp_to_int(fp_t val);           /* truncates toward zero */
int16_t fp_to_int_rounded(fp_t val);   /* rounds to nearest */
float   fp_to_float(fp_t val);

/* Arithmetic (overflow-safe via int64_t intermediates) */
fp_t fp_add(fp_t a, fp_t b);
fp_t fp_sub(fp_t a, fp_t b);
fp_t fp_mul(fp_t a, fp_t b);
fp_t fp_div(fp_t a, fp_t b);          /* b=0 returns FP_MAX/FP_MIN */

/* Comparison */
bool fp_gt(fp_t a, fp_t b);
bool fp_lt(fp_t a, fp_t b);
bool fp_eq(fp_t a, fp_t b);

/* Math functions */
fp_t fp_abs(fp_t val);
fp_t fp_min(fp_t a, fp_t b);
fp_t fp_max(fp_t a, fp_t b);
fp_t fp_clamp(fp_t val, fp_t lo, fp_t hi);
fp_t fp_lerp(fp_t a, fp_t b, fp_t t);

/* String formatting */
int fp_to_string(fp_t val, char *buf, uint16_t buf_size,
                 uint8_t decimal_places);
```

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (pure compute, expected trivial)

## Known issues

None. Pure stateless computation.

## How to test

```bash
cd wheels/primitives/fixed_point/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_fixed_point
```

## Test status

- Unit tests (sim): **PASS** (18 tests, 0 failures)
- Test coverage: conversion roundtrip, arithmetic, overflow-safe mul,
  div-by-zero, comparison, abs/min/max/clamp, lerp, string formatting,
  buffer overflow guard, NULL pointer guard, constant validation
- HIL: N/A (pure compute)

## API convention deviations

None. Prefix `fp_` follows api-conventions.md naming rules.

## Constants

| Define     | Value          | Description     |
|------------|----------------|-----------------|
| `FP_ONE`   | `0x00010000`   | 1.0 in Q16.16   |
| `FP_HALF`  | `0x00008000`   | 0.5 in Q16.16   |
| `FP_PI`    | `205887`       | 3.14159 approx  |
| `FP_MAX`   | `INT32_MAX`    | Saturated max   |
| `FP_MIN`   | `INT32_MIN`    | Saturated min   |

## API review

- **Date**: 2026-05-01
- **Key decisions**:
  - Q16.16 format (16 integer + 16 fraction bits)
  - mul/div use int64_t intermediates to prevent overflow
  - div-by-zero returns FP_MAX or FP_MIN based on sign of numerator
  - fp_abs(FP_MIN) returns FP_MAX (safe two's complement handling)
  - fp_to_string supports 0-5 decimal places

## Responsible human

@L-X-Yao
