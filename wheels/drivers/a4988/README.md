# a4988 — Allegro A4988 stepper motor driver (GPIO)

**Status: candidate**
**Layer: L1 (drivers)**

Step/dir stepper motor driver for Allegro A4988. Supports enable/disable,
direction control, single-step and multi-step with inter-step delay,
and 5 microstepping modes (full through 1/16).

## Origin

Proactive build for CNC, 3D printer, and robotics projects. A4988 is the
most common budget stepper driver (also compatible with DRV8825 pin-out).

## Why self-written?

No meaningful open-source C99 embedded driver exists. A4988 is a pure GPIO
driver (no communication protocol) so the implementation is trivial.

## Dependencies

- `gpio_hal` (callback injection, no concrete dependency)

## API

```c
a4988_err_t a4988_init(a4988_t *dev, const a4988_hal_t *hal, const a4988_cfg_t *cfg);
a4988_err_t a4988_enable(a4988_t *dev, bool on);
a4988_err_t a4988_set_direction(a4988_t *dev, a4988_dir_t dir);
a4988_err_t a4988_step(a4988_t *dev);
a4988_err_t a4988_step_n(a4988_t *dev, uint32_t count, uint32_t delay_us_between);
a4988_err_t a4988_set_microstep(a4988_t *dev, a4988_microstep_t mode);
```

## Configuration (compile-time)

None. Pin assignments via `a4988_cfg_t` at runtime.

## Supported platforms

- [x] Host (sim) -- 2026-05-11
- [ ] Real hardware (A4988 module) -- not yet verified

## Known issues

- No acceleration profile; use `stepper` wheel for trapezoidal acceleration.
- Step pulse width is fixed at 2us (above 1us minimum).

## How to test

```bash
cd wheels/drivers/a4988/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_a4988
```

## Test status

- Unit tests (sim): 26 pass
- Null tests: 8 pass
- HIL: N/A

## API convention deviations

None.

## API Review

Pending.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@L-X-Yao
