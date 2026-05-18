# signal_stats

**Status: candidate**
**Layer: L0 (primitives)**

Running statistics over a signal stream: mean, variance, min, max, count,
and range. Uses Welford's online algorithm for numerically stable variance
computation. Pure C99, no heap, no OS, fully reentrant per-context.

## Why not VSF native?

VSF has no online statistics primitive. Welford's algorithm is the correct
approach for embedded -- single-pass, no large buffers, no catastrophic
cancellation in variance.

## API

```c
#include "signal_stats.h"

signal_stats_err_t signal_stats_init(signal_stats_t *ctx);
signal_stats_err_t signal_stats_feed(signal_stats_t *ctx, float sample);
float              signal_stats_mean(const signal_stats_t *ctx);
float              signal_stats_variance(const signal_stats_t *ctx);
float              signal_stats_min(const signal_stats_t *ctx);
float              signal_stats_max(const signal_stats_t *ctx);
uint32_t           signal_stats_count(const signal_stats_t *ctx);
float              signal_stats_range(const signal_stats_t *ctx);
signal_stats_err_t signal_stats_reset(signal_stats_t *ctx);
```

## Usage

```c
signal_stats_t stats;
signal_stats_init(&stats);

for (int i = 0; i < n; i++) {
    signal_stats_feed(&stats, sensor_readings[i]);
}
float avg = signal_stats_mean(&stats);
float var = signal_stats_variance(&stats);
```

## Dependencies

None. Pure C99.

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Test status

- Unit tests (sim): PASS
- HIL: N/A
