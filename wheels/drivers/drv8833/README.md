# drv8833 — dual H-bridge DC motor driver

**Status: candidate**
**Layer: L1 (drivers)**

Controls two DC motors via 4 PWM pins. Supports forward, reverse, brake (slow
decay), and coast (fast decay) per channel. Optional nSLEEP pin control.
Compatible with DRV8833 and DRV8835 (IN/IN mode).
## Origin

Proactive build for robotics / mechatronics projects needing DC motor control.


## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. GPIO-only, trivial. Self-written with HAL injection per platform conventions.

## Dependencies

None. HAL callbacks injected by caller.

## API

```c
drv8833_err_t drv8833_init(drv8833_t *dev, const drv8833_hal_t *hal,
                           uint16_t max_duty);
drv8833_err_t drv8833_set_speed(drv8833_t *dev, drv8833_channel_t ch,
                                int16_t speed);
drv8833_err_t drv8833_brake(drv8833_t *dev, drv8833_channel_t ch);
drv8833_err_t drv8833_coast(drv8833_t *dev, drv8833_channel_t ch);
void          drv8833_sleep(drv8833_t *dev);
void          drv8833_wake(drv8833_t *dev);
```

### HAL contract

```c
typedef struct {
    void (*set_pwm)(void *ctx, uint8_t channel, uint8_t pin, uint16_t duty);
    void (*set_sleep)(void *ctx, bool sleep);  /* optional, may be NULL */
    void *ctx;
} drv8833_hal_t;
```

- `channel`: 0 = A, 1 = B
- `pin`: 0 = IN1, 1 = IN2
- `duty`: 0..max_duty

### Control modes

| Mode    | IN1       | IN2       | Decay |
|---------|-----------|-----------|-------|
| Forward | PWM(duty) | LOW       | —     |
| Reverse | LOW       | PWM(duty) | —     |
| Brake   | HIGH      | HIGH      | slow  |
| Coast   | LOW       | LOW       | fast  |

## Configuration (compile-time)

No compile-time defines. All configuration is runtime via `drv8833_init()`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/drv8833/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_drv8833
```

## Test status

- Unit tests (sim): 21 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: HAL uses `set_pwm(ctx, channel, pin, duty)` rather than
  per-pin callbacks — keeps the HAL struct small (1 function for 4 pins).
  `set_sleep` is optional (NULL-safe) since many boards hardwire nSLEEP high.

## Responsible human

@L-X-Yao
