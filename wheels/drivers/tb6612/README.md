# tb6612 — Toshiba TB6612FNG dual H-bridge motor driver (GPIO+PWM)

**Status: candidate**
**Layer: L1 (drivers)**

Dual-channel H-bridge driver for Toshiba TB6612FNG. Controls two DC motors
with direction (GPIO) and speed (PWM). Supports forward, reverse, coast,
brake, and standby modes.

## Origin

Proactive build for robotics and motor-control projects. TB6612 is one of
the most common small motor drivers in hobby/educational robotics.

## Why self-written?

No meaningful open-source C99 embedded driver exists. TB6612 is simple
enough that a vendor library would add unnecessary complexity vs. a
clean HAL-injected implementation.

## Dependencies

- `gpio_hal` + `pwm_hal` (callback injection, no concrete dependency)

## API

```c
tb6612_err_t tb6612_init(tb6612_t *dev, const tb6612_hal_t *hal, const tb6612_cfg_t *cfg);
tb6612_err_t tb6612_set_speed(tb6612_t *dev, tb6612_motor_t motor, int16_t speed_permille);
tb6612_err_t tb6612_brake(tb6612_t *dev, tb6612_motor_t motor);
tb6612_err_t tb6612_standby(tb6612_t *dev, bool enable);
```

## Configuration (compile-time)

None. Pin assignments via `tb6612_cfg_t` at runtime.

## Supported platforms

- [x] Host (sim) -- 2026-05-11
- [ ] Real hardware (TB6612FNG module) -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/tb6612/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tb6612
```

## Test status

- Unit tests (sim): 22 pass
- Null tests: 6 pass
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
