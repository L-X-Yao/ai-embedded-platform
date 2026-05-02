# pca9685 — I2C 16-channel 12-bit PWM driver

**Status: candidate**
**Layer: L1 (drivers)**

NXP PCA9685 I2C-controlled 16-channel PWM driver. Supports raw on/off
counts, duty-cycle percentage, and servo pulse width control. Configurable
PWM frequency 24-1526 Hz with sleep/wake power management.

## Origin

Proactive build for robotics (servo control) and LED dimming applications.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. All C impls tied to wiringPi/Arduino. Self-written with HAL injection per platform conventions.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
pca9685_err_t pca9685_init(pca9685_t *dev, const pca9685_i2c_t *i2c,
                           uint8_t dev_addr, uint16_t freq_hz);

pca9685_err_t pca9685_set_pwm(pca9685_t *dev, uint8_t channel,
                              uint16_t on, uint16_t off);

pca9685_err_t pca9685_set_duty(pca9685_t *dev, uint8_t channel, float duty_pct);

pca9685_err_t pca9685_set_servo_us(pca9685_t *dev, uint8_t channel,
                                   uint16_t pulse_us);

pca9685_err_t pca9685_all_off(pca9685_t *dev);

pca9685_err_t pca9685_set_freq(pca9685_t *dev, uint16_t freq_hz);

pca9685_err_t pca9685_sleep(pca9685_t *dev);
pca9685_err_t pca9685_wake(pca9685_t *dev);
```

## Configuration (compile-time)

No compile-time configuration needed. All parameters are runtime.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/pca9685/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_pca9685
```

## Test status

- Unit tests (sim): 18 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Three output modes (raw counts, duty%, servo pulse us).
  Prescaler calculation uses integer rounding from 25MHz oscillator.
  Sleep/wake exposed for power management.

## Responsible human

@user
