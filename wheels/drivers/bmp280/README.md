# bmp280

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Bosch BMP280 digital temperature and pressure
sensor. I2C interface injected via callbacks. Implements the official Bosch
32-bit integer compensation algorithm from datasheet BST-BMP280-DS001-26.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`bmp280`, `bmp2`, `bosch`, `barometer`, `pressure` — zero results.

## Origin

Proactive build. BMP280 is the most widely used barometric pressure sensor
in embedded applications (drones, weather stations, altimeters, HVAC).

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

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Known issues

- Uses forced mode on every read (not continuous). For high-frequency sampling,
  consider switching to normal mode (not yet implemented).
- BMP280 also supports SPI — only I2C is implemented.

## Tests

```bash
cd wheels/drivers/bmp280/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bmp280
```

26 assertions across 13 test cases: init OK, wrong chip ID rejected, I2C fail
on init, null safety (6 paths), temperature read with compensation, pressure
read with compensation, read I2C fail, reset command, reset I2C fail, alternate
address (0x77), 16x oversampling ctrl_meas encoding, calibration readback
verification, calibration I2C fail.

## Test status

- Unit tests (sim): PASS (13 C tests, 26 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Context struct (`bmp280_t`) holds calibration + I2C HAL + config — fully reentrant for multi-sensor setups; Bosch integer compensation (not floating-point) for MCU compatibility, converted to float at output; forced mode per-read to simplify state management; chip ID check on init to fail fast on wiring errors.

## Responsible human
@user
