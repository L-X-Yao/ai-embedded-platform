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

## Datasheet check

Checked on 2026-11-06 against Hitachi ADE-207-272(Z). The PCF8574 backpack pin map and addresses were not checked. Not run on hardware.

- **Matched:** Instruction codes, 4-bit reset order and waits, nibble order, bus timing.
- **Fixed:** init switched the display on before clearing it, and it accepted more than 20 columns on 3 and 4 row panels, past the end of DDRAM.
- **Suspected:** Row starts are fixed for 20 columns, so rows 2 and 3 of a 16x4 panel would land 4 columns off. A 16x1 panel with split addressing is not supported. `delay_ms` is called required, but init runs without it.

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

## API Review

Pending.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@L-X-Yao
