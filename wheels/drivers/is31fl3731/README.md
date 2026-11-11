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

## Why self-written?

No existing open-source component for IS31FL3731. Self-written for zero-dependency I2C
HAL injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


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

## Datasheet check

Checked on 2026-11-06 against IS31FL3731 Rev. G, 10/24/2024 (Lumissil Microsystems). Not run on hardware.

- **Matched:** Addresses, command register and pages, the frame map, LED index arithmetic, function registers, MODE, PFS, AE and SSD bits, 256 PWM steps.
- **Fixed:** `is31fl3731_init` set up only part of frame 0, so the rest of the frame registers stayed random and frames 1-7 could show random LEDs. It now writes LED Control, Blink Control and PWM of all 8 frames before leaving shutdown, which takes 1455 register writes instead of 170. The header called software shutdown hardware shutdown. `is31fl3731_set_led_enable` read LED Control while the chip ran, but the manual allows frame register reads only in software shutdown, so the write-back could corrupt the other 7 LEDs in that byte. It now works from `led_ctrl`, a shadow in `is31fl3731_t` that costs 144 bytes per instance, and never reads the chip. The model and mock returned frame data in any state, which hid this, and now return junk while the chip runs. This fix has not run on hardware.
- **Still open:** No blink, Auto Frame Play, Audio Frame Play, breath, AGC or SDB pin support. No driver function calls `read_regs` any more, but init still refuses NULL there.

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

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@L-X-Yao
