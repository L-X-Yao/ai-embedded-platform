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

## Datasheet check

Checked on 2026-11-06 against D-FT6x06-1207-V01 (Version: 0.1, Jul. 23, 2012, preliminary) and the Application Note for FT6x06 CTPM (Version 0.1, Jul 26, 2012). Both cover the sibling FT6x06 family: no FT6236/FT6336 document could be downloaded. Not run on hardware.

- **Matched:** Per the note: DEV_MODE, GEST_ID, TD_STATUS, point, TH_GROUP and FIRMID addresses, the point field layout, the event encoding, the 11-byte burst. Address 0x38 and chip IDs 0x36 and 0x64 are in neither FocalTech document, only in the Adafruit_FT6206 and Arduino-FT6336U headers.
- **Fixed:** `ft6x36_init` and `ft6x36_get_chip_id` read the chip ID from 0xA8, the panel ID register, so a real part would have failed with `FT6X36_ERR_ID`. They now read 0xA3. The source is the sibling family's note plus the Adafruit_FT6206 and Arduino-FT6336U headers, because no FT6236/FT6336 document could be downloaded.
- **Still open:** No RSTN control. The header claims gesture detection, but no API reads GEST_ID. Touch ID, weight and area are not exposed, so a host cannot track fingers across frames. G_MODE, CTRL, PERIODACTIVE and PWR_MODE are not configurable. Points are decoded only when the count is at least 1, so if the part reports a lift with TD_STATUS 0, `FT6X36_EVT_RELEASE` never appears.
- **Suspected:** A count above 2 is clamped and read as two touches. Event 11b, No event, comes out as an unnamed value 3 on an active point. CTRL stays at its default, so polled reads could fail once the panel drops to Monitor mode. Init does not wait the 300 ms after power-on. The model never sets Touch ID bits or the 0xFF defaults.

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

- Unit tests (sim): 28 pass (main) + 9 pass (null) + 17 pass (hw_model) = 54 total
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Register-based I2C HAL (write_reg/read_regs), touch data struct with event enum, chip ID validation on init

## Responsible human

@user
