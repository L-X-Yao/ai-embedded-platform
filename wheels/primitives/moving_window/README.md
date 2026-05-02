# moving_window — sliding window statistics (min/max/mean/variance)

**Status: candidate**
**Layer: L0 (primitives)**

Maintains min, max, mean, and population variance over a fixed-size sliding window.
Caller provides the float storage array. Pure C99, no heap, fully reentrant per-context.
Different from `moving_average` which only tracks mean (SMA/EMA).

## Origin

Proactive build for sensor data analysis use cases (signal quality, jitter monitoring).

## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None.

## API

```c
moving_window_err_t moving_window_init(moving_window_t *ctx, float *buf, size_t capacity);
moving_window_err_t moving_window_push(moving_window_t *ctx, float value);
float               moving_window_get_mean(const moving_window_t *ctx);
float               moving_window_get_min(const moving_window_t *ctx);
float               moving_window_get_max(const moving_window_t *ctx);
float               moving_window_get_variance(const moving_window_t *ctx);
size_t              moving_window_get_count(const moving_window_t *ctx);
bool                moving_window_is_full(const moving_window_t *ctx);
moving_window_err_t moving_window_reset(moving_window_t *ctx);
```

## Configuration (compile-time)

None. Window size is set at runtime via `init(capacity)`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

- Min/max are computed by scanning the buffer on each query (O(n)).
  For very large windows this may matter; for typical embedded use (n < 64) it is fine.
- Running sum of squares may lose precision with very large values over many rollovers
  due to float cancellation. Use double storage if sub-ppm precision is needed.

## How to test

```bash
cd wheels/primitives/moving_window/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_moving_window
```

## Test status

- Unit tests (sim): 16 tests, all pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: O(n) min/max scan chosen over O(1) monotonic deque to keep code simple and small (~100 lines). Caller-provided float array, no heap.

## Responsible human

@user

## Why not moving_average?

`moving_average` provides SMA (mean only) and EMA. This wheel adds min, max, and variance
tracking over the same window, which is needed for signal quality assessment, outlier
detection, and sensor health monitoring.
