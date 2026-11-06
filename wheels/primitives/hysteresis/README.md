# hysteresis

**Status: candidate**
**Layer: L0 (primitives)**

Schmitt-trigger-style threshold comparator with configurable hysteresis band.
Switches HIGH when value >= thresh_high, LOW when value <= thresh_low.
Values inside the dead band keep the previous state. NaN inputs are ignored.
Pure C99, no heap, no OS.

## Origin

Proactive build (ADR-011). Schmitt-trigger-style threshold comparator with configurable hysteresis band.


## Why self-written?

existing open-source has no software hysteresis comparator. This is a common signal-conditioning
primitive needed by sensor drivers and alarm logic across projects.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

None. Pure C99.

## API

```c
#include "hysteresis.h"

hysteresis_err_t hysteresis_init(hysteresis_t *ctx, float thresh_low, float thresh_high);
bool             hysteresis_feed(hysteresis_t *ctx, float value);
bool             hysteresis_get_state(const hysteresis_t *ctx);
hysteresis_err_t hysteresis_reset(hysteresis_t *ctx);
hysteresis_err_t hysteresis_set_thresholds(hysteresis_t *ctx,
                                           float thresh_low, float thresh_high);
```

## Usage

```c
hysteresis_t h;
hysteresis_init(&h, 25.0f, 30.0f);  /* ON above 30, OFF below 25 */

bool fan_on = hysteresis_feed(&h, temperature);
if (fan_on) gpio_set(FAN_PIN);
else        gpio_clear(FAN_PIN);
```

## Supported platforms

- [x] Host simulation (gcc/cmake)

## Known issues

None.


## How to test

```bash
cd wheels/primitives\hysteresis/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hysteresis
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
