# signal_stats

**Status: candidate**
**Layer: L0 (primitives)**

Running statistics over a signal stream: mean, variance, min, max, count,
and range. Uses Welford's online algorithm for numerically stable variance
computation. Pure C99, no heap, no OS, fully reentrant per-context.

## Origin

Proactive build (ADR-011). Running statistics over a signal stream: mean, variance, min, max, count,


## Why self-written?

existing open-source has no online statistics primitive. Welford's algorithm is the correct
approach for embedded -- single-pass, no large buffers, no catastrophic
cancellation in variance.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

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

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/primitives\signal_stats/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_signal_stats
```


## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.


## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
