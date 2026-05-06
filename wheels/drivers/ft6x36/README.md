# ft6x36 — FocalTech FT6236/FT6336 capacitive touch controller (I2C)

**Status: candidate**
**Layer: L1 (drivers)**

Capacitive touch controller driver supporting 1-2 simultaneous touch points,
touch event detection (press/release/contact), configurable threshold, and
chip identification. Supports both FT6236 (chip ID 0x36) and FT6336 (chip ID 0x64).

## Origin

Proactive build for touch-screen interfaces (HMI panels, LCD+touch combos).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- Existing FT6x36 drivers are typically tied to Arduino/ESP-IDF frameworks.
- No suitable zero-dependency C99 implementation found.

Reference: FocalTech FT6236 datasheet.

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
ft6x36_err_t ft6x36_init(ft6x36_t *dev, const ft6x36_i2c_t *i2c);
ft6x36_err_t ft6x36_read_touch(ft6x36_t *dev, ft6x36_touch_t *out);
ft6x36_err_t ft6x36_set_threshold(ft6x36_t *dev, uint8_t value);
ft6x36_err_t ft6x36_get_chip_id(ft6x36_t *dev, uint8_t *out);
ft6x36_err_t ft6x36_get_firmware_version(ft6x36_t *dev, uint8_t *out);
```

## Configuration (compile-time)

No compile-time configuration needed.

## Supported platforms

- [x] Host (sim) — 2026-05-06
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ft6x36/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ft6x36
./build/test_null_ft6x36
./build/test_hw_model_ft6x36
```

## Test status

- Unit tests (sim): 27 pass (main) + 9 pass (null) + 17 pass (hw_model) = 53 total
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Register-based I2C HAL (write_reg/read_regs), touch data struct with event enum, chip ID validation on init

## Responsible human

@user
