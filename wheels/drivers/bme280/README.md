# bme280

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Bosch BME280 digital temperature, humidity, and
pressure sensor. I2C interface injected via callbacks. Implements the official
Bosch 32-bit integer compensation algorithm from datasheet BST-BME280-DS002-15.
Extends BMP280 with humidity measurement (registers 0xFD-0xFE).
## Origin

Proactive build. BME280 is the most widely used environmental sensor in
embedded applications (weather stations, HVAC, indoor air quality, drones).

## Dependencies

I2C HAL (injected via `bme280_i2c_t` callbacks). No other dependencies.

## API

```c
bme280_err_t bme280_init(bme280_t *dev, const bme280_i2c_t *i2c,
                         const bme280_cfg_t *cfg);
bme280_err_t bme280_read(bme280_t *dev, bme280_data_t *out);
bme280_err_t bme280_set_oversampling(bme280_t *dev,
                                     bme280_oversampling_t osrs_t,
                                     bme280_oversampling_t osrs_p,
                                     bme280_oversampling_t osrs_h);
bme280_err_t bme280_reset(bme280_t *dev);
```

Output: `bme280_data_t { float temperature_c; float pressure_pa; float humidity_pct; }`

## Configuration (compile-time)

None. Oversampling and I2C address are runtime-configurable via `bme280_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Known issues

- Uses forced mode on every read (not continuous). For high-frequency sampling,
  consider switching to normal mode (not yet implemented).
- BME280 also supports SPI — only I2C is implemented.

## How to test

```bash
cd wheels/drivers/bme280/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bme280
```

## Test status

- Unit tests (sim): PASS (19 C tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Context struct (`bme280_t`) holds calibration + I2C HAL + config — fully reentrant for multi-sensor setups; Bosch integer compensation (not floating-point) for MCU compatibility, converted to float at output; forced mode per-read to simplify state management; chip ID check (0x60) on init to fail fast and distinguish from BMP280 (0x58); humidity calibration split across two register blocks (0xA1 + 0xE1..0xE7) per datasheet; `set_oversampling` allows runtime reconfiguration without re-init.

## Responsible human
@user
