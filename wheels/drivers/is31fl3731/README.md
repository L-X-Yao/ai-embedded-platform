# is31fl3731 -- ISSI IS31FL3731 LED matrix controller driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the ISSI IS31FL3731 LED matrix controller.
I2C interface injected via callbacks. Controls up to 144 LEDs (9x16 matrix)
across 8 display frames with individual PWM brightness. Supports frame
selection, per-LED enable/disable, fill, and shutdown mode.

## Origin

Proactive build. IS31FL3731 is used in Adafruit CharliePlex LED matrices
and custom LED display panels for embedded HMI.

## Why not VSF native?

No VSF component for IS31FL3731. Self-written for zero-dependency I2C
HAL injection and platform conventions.

## Dependencies

I2C HAL (injected via `is31fl3731_i2c_t` callbacks). No other dependencies.

## API

```c
is31fl3731_err_t is31fl3731_init(is31fl3731_t *dev,
                                 const is31fl3731_i2c_t *i2c,
                                 const is31fl3731_cfg_t *cfg);
is31fl3731_err_t is31fl3731_set_frame(is31fl3731_t *dev, uint8_t frame);
is31fl3731_err_t is31fl3731_set_pixel(is31fl3731_t *dev, uint8_t frame,
                                      uint8_t led_idx, uint8_t pwm);
is31fl3731_err_t is31fl3731_set_led_enable(is31fl3731_t *dev, uint8_t frame,
                                           uint8_t led_idx, bool enable);
is31fl3731_err_t is31fl3731_fill(is31fl3731_t *dev, uint8_t frame, uint8_t pwm);
is31fl3731_err_t is31fl3731_display_frame(is31fl3731_t *dev, uint8_t frame);
is31fl3731_err_t is31fl3731_shutdown(is31fl3731_t *dev, bool enable);
```

## Usage

```c
is31fl3731_t led;
is31fl3731_cfg_t cfg = { .addr = IS31FL3731_ADDR_BASE };
is31fl3731_init(&led, &i2c_hal, &cfg);

is31fl3731_set_pixel(&led, 0, 42, 128);   /* LED 42 at half brightness */
is31fl3731_display_frame(&led, 0);
```

## Configuration (compile-time)

None. I2C address is runtime-configurable via `is31fl3731_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

None.

## How to test

```bash
cd wheels/drivers/is31fl3731/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_is31fl3731
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## Responsible human

@user
