# scale_map

**Status: candidate**
**Layer: L0 (primitives)**

Linear value mapping from one range to another, like Arduino's `map()`.
Integer (int32) and float variants, each with an optional clamped version
that constrains output to [out_min, out_max]. All functions are stateless,
thread-safe, and ISR-safe. Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). Linear value mapping from one range to another, like Arduino's `map()`.


## Why self-written?

existing open-source has no standalone range-mapping utility. This is a ubiquitous embedded
primitive (ADC scaling, servo mapping, sensor calibration) worth standardizing.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "scale_map.h"

int32_t scale_map_i32(int32_t x, int32_t in_min, int32_t in_max,
                      int32_t out_min, int32_t out_max);
int32_t scale_map_i32_clamp(int32_t x, int32_t in_min, int32_t in_max,
                            int32_t out_min, int32_t out_max);
float   scale_map_f(float x, float in_min, float in_max,
                    float out_min, float out_max);
float   scale_map_f_clamp(float x, float in_min, float in_max,
                          float out_min, float out_max);
```

## Usage

```c
/* Map 12-bit ADC (0-4095) to temperature (0-100 C) */
int32_t temp = scale_map_i32(adc_val, 0, 4095, 0, 100);

/* Map joystick to servo with clamping */
int32_t servo_us = scale_map_i32_clamp(joy, -512, 511, 1000, 2000);
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/primitives\scale_map/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_scale_map
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
