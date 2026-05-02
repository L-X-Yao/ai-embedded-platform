# servo

**Status: candidate**
**Layer: L1 (drivers)**

PWM servo motor driver. Maps angle (0-180° or configurable) to pulse width
(typically 500-2500µs). Caller provides PWM callback, no timer HW dependency.

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. PWM-only timing; no chip driver to vendor. Self-written with HAL injection per platform conventions.

## Dependencies

PWM HAL (injected via callback).

## API

```c
servo_err_t servo_init(servo_t *dev, const servo_pwm_t *pwm, const servo_cfg_t *cfg);
servo_err_t servo_set_angle(servo_t *dev, uint16_t angle);
servo_err_t servo_set_pulse(servo_t *dev, uint32_t pulse_us);
uint16_t    servo_get_angle(const servo_t *dev);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/drivers/servo/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **8 C cases** — init null/bad config, angles 0/90/180, clamp, HAL failure, direct pulse

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
