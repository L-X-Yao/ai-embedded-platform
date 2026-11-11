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

## Datasheet check

Checked on 2026-11-06 against NXP PCA9685 Product data sheet, Rev. 4, 16 April 2015. Not run on hardware.

- **Matched:** The default address, register map, channel layout, MODE1 bits, full ON and full OFF bits, the prescale formula and its limits, PRE_SCALE written only in sleep, the 1 ms wake wait, the shutdown in `pca9685_all_off`.
- **Fixed:** `pca9685_init` left the chip asleep, so no channel could produce PWM. The waking write could carry RESTART before the 500 us were up. `pca9685_set_pwm` accepted equal ON and OFF counts, and `pca9685_set_duty` and `pca9685_set_servo_us` could reach them with a count of 0. `delay_ms` was optional although the wake needs it, so init now returns `PCA9685_ERR_PARAM`. `pca9685_set_duty` scaled by 4095 and not 4096. A comment called a MODE1 write a software reset. A second `pca9685_sleep` wrote RESTART back as a 1, which clears it, so the channels did not resume on wake. The model reset every LEDn_OFF_H to 0, stored RESTART as a plain bit and left ALL_LED writes out of the channels, and the HAL delay took no time, so it now follows the manual on all of these.
- **Still open:** Init is no reset, so after a restart of the MCU alone MODE2 and the LED registers keep their state and old outputs can run on. MODE2 is never set, so OUTDRV, INVRT, OCH and OUTNE stay at their reset values. There is no EXTCLK or SWRST support, and the board must hold OE low. `dev_addr` is not checked against reserved addresses such as LED All Call 0x70. `pca9685_set_servo_us` uses the requested frequency, not the one the prescaler gives, about 0.3 % on the pulse at 60 Hz.
- **Suspected:** `pca9685_set_pwm` writes four registers in four transfers with OCH=0, so a count change can pass through mixed values and glitch a servo pulse. The manual does not give the pin level in sleep, so `pca9685_sleep` may not turn outputs off as the header says.

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

@L-X-Yao
