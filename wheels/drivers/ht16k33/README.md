# ht16k33 — HT16K33 I2C LED matrix/segment controller driver

**Status: candidate**
**Layer: L1 (drivers)**

Holtek HT16K33 LED controller driver. Supports 16x8 LED matrix with 16-byte
display RAM, 16-level PWM brightness control, and I2C addressing for up to 8
devices (0x70-0x77). Command-based protocol: no chip ID register.

## Origin

Proactive build for LED matrix/7-segment display use cases (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- Adafruit_LED_Backpack (C++, Arduino-only, heavy class hierarchy)
- Generic HT16K33 libs are Arduino/C++ or platform-locked
- No suitable zero-dependency C99 implementation found

Reference: HT16K33 datasheet (Holtek).

## Dependencies

None (I2C HAL injected by caller).

## API

```c
/* Init: oscillator on, display on, set brightness */
ht16k33_err_t ht16k33_init(ht16k33_t *dev,
                            const ht16k33_i2c_t *i2c,
                            const ht16k33_cfg_t *cfg);

/* Set brightness 0-15 (1/16 to 16/16 duty cycle) */
ht16k33_err_t ht16k33_set_brightness(ht16k33_t *dev, uint8_t level);

/* Write raw display RAM (1-16 bytes) */
ht16k33_err_t ht16k33_write_raw(ht16k33_t *dev,
                                const uint8_t *buf, uint8_t len);

/* Clear all 16 display RAM bytes */
ht16k33_err_t ht16k33_clear(ht16k33_t *dev);
```

## Configuration (compile-time)

No compile-time configuration. Address and brightness set at runtime via `ht16k33_cfg_t`.

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against HT16K33 RAM Mapping 16*8 LED Controller Driver with keyscan, Revision V1.10, May 16, 2011. Not run on hardware.

- **Matched:** Addresses 0x70-0x77, system setup 0x21 sent first, display setup 0x81, dimming 0xE0 plus 0-15, the 16-byte display RAM.
- **Fixed:** The model reset brightness to 0, where the manual gives 16/16 duty, accepted commands and RAM writes in standby, and denied the readable INT flag and key data. It now resets to level 15, ignores those writes in standby, and models both.
- **Still open:** No blink, standby, ROW/INT, key scan or INT flag support. RAM is written one byte per transaction, without the page write.
- **Suspected:** Commands go out through `write_reg` with a trailing 0x00 the manual never says is ignored. Init turns the display on before dimming and never clears RAM, so old RAM content can show at full brightness. Init does not wait the 1 ms after power-on.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ht16k33/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ht16k33
./build/test_hw_model_ht16k33
```

## Test status

- Unit tests (sim): 32 tests + 12 hw_model tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: Command-based (no chip ID), write_reg for both commands and RAM, cfg struct for addr+brightness

## Responsible human

@L-X-Yao
