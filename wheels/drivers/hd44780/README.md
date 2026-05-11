# hd44780 — HD44780 character LCD driver via PCF8574 I2C backpack

**Status: candidate**
**Layer: L1 (drivers)**

Drives HD44780-compatible character LCDs (16x2, 20x4) through a PCF8574 I2C GPIO
expander in 4-bit mode. Supports cursor positioning, character/string output, and
backlight control. Requires delay_ms callback for LCD timing.

## Origin

Proactive build for common character LCD use cases (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- LiquidCrystal_I2C (Arduino) is C++ and framework-dependent.
- No suitable zero-dependency C99 implementation found.

Reference: HD44780 datasheet (Hitachi), PCF8574 datasheet (NXP).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
hd44780_err_t hd44780_init(hd44780_t *dev, const hd44780_i2c_t *i2c,
                           uint8_t addr, uint8_t cols, uint8_t rows);
hd44780_err_t hd44780_clear(hd44780_t *dev);
hd44780_err_t hd44780_set_cursor(hd44780_t *dev, uint8_t col, uint8_t row);
hd44780_err_t hd44780_write_char(hd44780_t *dev, char ch);
hd44780_err_t hd44780_write_str(hd44780_t *dev, const char *str);
hd44780_err_t hd44780_backlight(hd44780_t *dev, bool on);
```

## Supported platforms

- [x] Host (sim) — 2026-05-11
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/hd44780/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_hd44780 && ./build/test_hw_model_hd44780 && ./build/test_null_hd44780
```

## Test status

- Unit tests (sim): 33 passed (21 unit + 4 hw_model + 8 null)
- HIL: N/A

## API convention deviations

None.

## Responsible human

@user
