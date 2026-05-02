# servo

**Status: candidate**
**Layer: L1 (drivers)**

PWM servo motor driver. Maps angle (0-180° or configurable) to pulse width
(typically 500-2500µs). Caller provides PWM callback, no timer HW dependency.
## API

```c
servo_err_t servo_init(servo_t *dev, const servo_pwm_t *pwm, const servo_cfg_t *cfg);
servo_err_t servo_set_angle(servo_t *dev, uint16_t angle);
servo_err_t servo_set_pulse(servo_t *dev, uint32_t pulse_us);
uint16_t    servo_get_angle(const servo_t *dev);
```

## Test status

- Unit tests: **8 C cases** — init null/bad config, angles 0/90/180, clamp, HAL failure, direct pulse

## Dependencies

PWM HAL (injected via callback).

## How to test

```bash
cd wheels/drivers/servo/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Origin

Proactive build for common embedded components (2026-04-29 batch).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Known issues

None.

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## API convention deviations

无。

## Responsible human

@user
