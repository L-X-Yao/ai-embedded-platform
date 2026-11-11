# ewma -- integer-only exponential weighted moving average

**Status: candidate**
**Layer: L0 (primitives)**

Integer-only EWMA filter using bit-shift alpha (1/2^shift) for MCUs
without FPU. Complements the float-based EMA in `moving_average/`.
First sample primes the accumulator; subsequent samples are smoothed.
Larger shift values produce slower tracking (more smoothing).

## Origin

Proactive build. Integer EWMA is essential for signal smoothing on
Cortex-M0/M0+ and other FPU-less MCUs (ADC filtering, sensor fusion).

## Why self-written?

No existing open-source equivalent. Textbook DSP primitive self-written for zero-dependency
platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None.

## API

```c
ewma_err_t ewma_init(ewma_t *ctx, uint8_t shift);
int32_t    ewma_feed(ewma_t *ctx, int32_t sample);
int32_t    ewma_get(const ewma_t *ctx);
ewma_err_t ewma_reset(ewma_t *ctx);
```

- `init()` -- shift range 1..16, represents alpha = 1/(2^shift)
- `feed()` -- feed sample, return filtered value
- `get()` -- return current value without feeding (0 if unprimed)
- All functions are ISR-safe

## Usage

```c
ewma_t filter;
ewma_init(&filter, 3);    /* alpha = 1/8 */

int32_t smoothed;
smoothed = ewma_feed(&filter, 1000);  /* primes to 1000 */
smoothed = ewma_feed(&filter, 1100);  /* 1000 + (1100-1000)/8 = 1012 */
smoothed = ewma_feed(&filter, 900);   /* tracks toward 900 */
```

## Configuration (compile-time)

None.

## Supported platforms

- [x] Host (sim)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/ewma/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ewma
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## Migration

### Scaled accumulator (2026-11)

- **What changed:** `ewma_t` holds the filter state scaled by 2^shift in a new 64-bit `acc`, and `value` is now only the last output, not the state. The struct grows from 8 to 16 bytes and needs 8-byte alignment. Code grows from 96 to 160 bytes on Cortex-M0 and from 86 to 200 on Cortex-M4. The numbers change too: the output now reaches a constant input exactly, in both directions, where it used to stop up to 2^shift - 1 short, and a step below 2^shift now moves it instead of being ignored.
- **What a caller has to do:** Use `ewma_feed` and `ewma_get`, never the fields: writing `value` no longer seeds the filter, so feed a first sample instead. Recheck thresholds tuned to the old offset. Do not restore an `ewma_t` saved by the old version.
- **If they do nothing:** Code using the API compiles unchanged and reads the true level, which is higher than before: shift 8 used to stop at 3840 for 4095. No code in this repo uses the wheel.

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@L-X-Yao
