# bmp280

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Bosch BMP280 digital temperature and pressure
sensor. I2C interface injected via callbacks. Implements the official Bosch
32-bit integer compensation algorithm from datasheet BST-BMP280-DS001-26.
## Origin

Proactive build. BMP280 is the most widely used barometric pressure sensor
in embedded applications (drones, weather stations, altimeters, HVAC).


## Why self-written?

Searched GitHub (2026-05-02). Referenced boschsensortec/BMP2_SensorAPI (BSD-3) for register sequences and compensation algorithms. Official Bosch driver; compensation formulas from BST-BMP280-DS001-26, cross-validated. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

I2C HAL (injected via `bmp280_i2c_t` callbacks). No other dependencies.

## API

```c
bmp280_err_t bmp280_init(bmp280_t *dev, const bmp280_i2c_t *i2c,
                         const bmp280_cfg_t *cfg);
bmp280_err_t bmp280_read(bmp280_t *dev, bmp280_data_t *out);
bmp280_err_t bmp280_reset(bmp280_t *dev);
```

Output: `bmp280_data_t { float temperature_c; float pressure_pa; }`

## Configuration (compile-time)

None. Oversampling and I2C address are runtime-configurable via `bmp280_cfg_t`.

Set both `osrs_t` and `osrs_p`, because 0 is `BMP280_OVERSAMPLING_SKIP`:

| `osrs_t` | `osrs_p` | `bmp280_read` |
|---|---|---|
| SKIP | SKIP | not reached: init returns `BMP280_ERR_PARAM` |
| set | SKIP | measures temperature only; `pressure_pa` is 0 |
| SKIP | set | measures pressure only; `temperature_c` is the temperature init measured, and the pressure is compensated with it |

With `osrs_t` SKIP, init takes one temperature measurement, and the pressure
loses accuracy as the real temperature moves away from it. Run init again to
refresh it.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against BST-BMP280-DS001-26, document revision 1.26, October 2021. Not run on hardware.

- **Matched:** Addresses, chip ID, register addresses, the reset word, ctrl_meas fields, oversampling codes, the measuring bit, the calibration table, the 6-byte burst, and temperature and pressure compensation against section 8.2.
- **Fixed:** `bmp280_read` compensated the 0x80000 placeholder of a skipped channel and returned it as a measurement, and a skipped temperature also shifted pressure. A skipped pressure now reads 0, a skipped temperature reads the one init measured, and both skipped is refused. The file header cited section 8.1, the floating-point code, for the fixed-point code in 8.2. The model wrote real data for skipped channels and credited its calibration to an appendix that prints none. None of this has run on hardware.
- **Still open:** The model measures nothing for mode 10, which the manual makes forced, starts the data registers at 0x00 where the manual gives 0x80, finishes a conversion inside the ctrl_meas write and keeps any value written to 0xE0, so the tests cannot see the status poll. Normal mode, the IIR filter, t_sb and SPI are not implemented. im_update is never read, and the 2 ms start-up after power-on is left to the caller.
- **Suspected:** The status poll never calls `delay_ms`, so x16 settings can time out on a 400 kHz bus. The first status read can see measuring 0 before the conversion starts and return the previous sample, which with temperature skipped would spoil init's one measurement. `bmp280_reset` returns without waiting for the reset to finish. Pressure uses the reduced-accuracy section 8.2 routine although 64-bit integers are in use, about 3 Pa from the 64-bit code at the reference point.

## Known issues

- Uses forced mode on every read (not continuous). For high-frequency sampling,
  consider switching to normal mode (not yet implemented).
- BMP280 also supports SPI — only I2C is implemented.

## How to test

```bash
cd wheels/drivers/bmp280/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bmp280
```

17 test cases, among them: init OK, wrong chip ID rejected, I2C fail
on init, null safety (6 paths), temperature read with compensation, pressure
read with compensation, read I2C fail, reset command, reset I2C fail, alternate
address (0x77), 16x oversampling ctrl_meas encoding, calibration readback
verification, calibration I2C fail.

## Test status

- Unit tests (sim): PASS (17 C tests, 20 hw_model tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Context struct (`bmp280_t`) holds calibration + I2C HAL + config — fully reentrant for multi-sensor setups; Bosch integer compensation (not floating-point) for MCU compatibility, converted to float at output; forced mode per-read to simplify state management; chip ID check on init to fail fast on wiring errors.

## Responsible human
@user
