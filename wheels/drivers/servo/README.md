# servo

**STATUS: candidate**
**Layer: L1 (drivers)**

PWM servo motor driver. Maps angle (0-180° or configurable) to pulse width
(typically 500-2500µs). Caller provides PWM callback, no timer HW dependency.

## Why not VSF native?

Checked VSF source (2026-04-29): no servo driver in VSF.

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
