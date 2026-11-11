# peak_detector

**Status: candidate**
**Layer: L0 (primitives)**

Tracks running peak (max) and valley (min) of a signal stream with
configurable hold/decay. Decay rate controls how quickly the held extremes
relax toward the current value: 0 = infinite hold, 1 = no hold.
Pure C99, no heap, no OS, fully reentrant.

## Origin

Proactive build (ADR-011). Tracks running peak (max) and valley (min) of a signal stream with


## Why self-written?

existing open-source has no peak/valley tracking primitive. This is a common building block
for audio level meters, vibration monitoring, and envelope detection.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "peak_detector.h"

peak_detector_err_t peak_detector_init(peak_detector_t *ctx, float decay_rate);
peak_detector_err_t peak_detector_feed(peak_detector_t *ctx, float sample);
float               peak_detector_get_peak(const peak_detector_t *ctx);
float               peak_detector_get_valley(const peak_detector_t *ctx);
float               peak_detector_get_range(const peak_detector_t *ctx);
peak_detector_err_t peak_detector_reset(peak_detector_t *ctx);
```

## Usage

```c
peak_detector_t pd;
peak_detector_init(&pd, 0.01f);  /* slow decay */

for (int i = 0; i < n; i++) {
    peak_detector_feed(&pd, adc_samples[i]);
}
float range = peak_detector_get_range(&pd);  /* peak - valley */
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/primitives\peak_detector/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_peak_detector
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

@L-X-Yao
