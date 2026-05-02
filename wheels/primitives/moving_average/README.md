# moving_average

**Status: candidate**
**Layer: L0 (primitives)**

Sliding window (SMA) and exponential (EMA) moving average filters for signal
smoothing. Pure C99, no heap, fully reentrant. Caller provides the SMA window
buffer; EMA needs no buffer.
## Origin

Proactive build. Moving average is the most common signal filter in embedded:
ADC smoothing, sensor noise rejection, PID input conditioning.


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None. Pure C99 with `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>`.

## API

```c
/* Simple Moving Average — exact window average */
moving_average_err_t moving_average_sma_init(moving_average_sma_t *ctx,
                                             float *buf, size_t window);
moving_average_err_t moving_average_sma_reset(moving_average_sma_t *ctx);
float                moving_average_sma_feed(moving_average_sma_t *ctx, float sample);
float                moving_average_sma_get(const moving_average_sma_t *ctx);
bool                 moving_average_sma_is_full(const moving_average_sma_t *ctx);

/* Exponential Moving Average — single-pole IIR, no buffer */
moving_average_err_t moving_average_ema_init(moving_average_ema_t *ctx, float alpha);
moving_average_err_t moving_average_ema_reset(moving_average_ema_t *ctx);
float                moving_average_ema_feed(moving_average_ema_t *ctx, float sample);
float                moving_average_ema_get(const moving_average_ema_t *ctx);
```

## Configuration (compile-time)

None. Window size and alpha are runtime parameters.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (no hardware dependency expected)

## Known issues

None.

## How to test

```bash
cd wheels/primitives/moving_average/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_moving_average
```

48 assertions across 17 test cases: SMA init, null safety (7 paths), single sample,
progressive filling with partial averages, sliding window eviction, reset, constant
input stability, window=1 passthrough, empty get; EMA init, null safety (6 paths),
first sample seeding, smoothing arithmetic, alpha=1 passthrough, convergence to
constant, reset, get before/after feed.

## Test status

- Unit tests (sim): PASS (17 C tests, 48 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Two filter types in one wheel (SMA + EMA) because they share the same use case domain; SMA uses caller-provided buffer (no heap, size known at init); EMA first sample seeds value directly (no lag on startup); alpha must be (0, 1] — 0 rejected because it would mean "ignore all input".

## Responsible human
@user
