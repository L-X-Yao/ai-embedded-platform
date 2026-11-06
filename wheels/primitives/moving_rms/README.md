# moving_rms

**Status: candidate**
**Layer: L0 (primitives)**

Running root-mean-square over a sliding window with O(1) per-sample updates.
Maintains a sum-of-squares in a circular buffer provided by the caller.
Useful for vibration monitoring, AC signal measurement, and audio level
detection. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). Running root-mean-square over a sliding window with O(1) per-sample updates.


## Why self-written?

existing open-source has no sliding-window RMS primitive. This fills a common need for
real-time signal analysis on embedded targets without heap allocation.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99 with `<math.h>` for `sqrtf`.

## API

```c
#include "moving_rms.h"

moving_rms_err_t moving_rms_init(moving_rms_t *ctx, float *buf, size_t window);
float            moving_rms_feed(moving_rms_t *ctx, float sample);
float            moving_rms_get(const moving_rms_t *ctx);
moving_rms_err_t moving_rms_reset(moving_rms_t *ctx);
```

## Usage

```c
float window_buf[64];
moving_rms_t rms;
moving_rms_init(&rms, window_buf, 64);

/* Feed accelerometer samples in a loop */
float rms_val = moving_rms_feed(&rms, accel_x);
if (rms_val > VIBRATION_THRESHOLD) {
    set_alarm();
}
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

- After a sample far larger than the rest leaves the window, float rounding can leave the RMS off until `head` next wraps, at most one window later.


## How to test

```bash
cd wheels/primitives\moving_rms/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_moving_rms
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
