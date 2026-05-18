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

## Why not VSF native?

No VSF equivalent. Textbook DSP primitive self-written for zero-dependency
platform conventions.

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

## API convention deviations

None.

## Responsible human

@user
