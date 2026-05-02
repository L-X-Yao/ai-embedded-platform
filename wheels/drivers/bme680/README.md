# bme680

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Bosch BME680 environmental sensor: temperature,
humidity, pressure, and gas resistance (air quality). I2C interface injected
via callbacks. Implements the Bosch integer compensation algorithm from
datasheet BST-BME680-DS001-00. Gas resistance uses lookup tables from the
Bosch reference driver.
## Origin

Proactive build. BME680 is the standard environmental sensor for air quality
monitoring, weather stations, indoor navigation, and IoT applications.
Extends the existing bmp280 wheel with humidity and gas resistance channels.


## Why self-written?

Searched GitHub (2026-05-02). Referenced boschsensortec/BME68x_SensorAPI (BSD-3) for register sequences and compensation algorithms. Official Bosch driver; gas resistance algorithm referenced from datasheet BST-BME680-DS001-00. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

I2C HAL (injected via `bme680_i2c_t` callbacks). No other dependencies.

## API

```c
bme680_err_t bme680_init(bme680_t *dev, const bme680_i2c_t *i2c, uint8_t addr);
bme680_err_t bme680_read(bme680_t *dev, bme680_data_t *data);
bme680_err_t bme680_set_oversampling(bme680_t *dev, uint8_t os_temp,
                                     uint8_t os_hum, uint8_t os_pres);
bme680_err_t bme680_set_gas_heater(bme680_t *dev, uint16_t temp_c,
                                   uint16_t duration_ms);
bme680_err_t bme680_reset(bme680_t *dev);
```

Output: `bme680_data_t { float temperature_c; float humidity_pct; float pressure_pa; float gas_resistance_ohm; }`

- `temperature_c`: degrees Celsius
- `humidity_pct`: percent RH (0..100)
- `pressure_pa`: Pascals
- `gas_resistance_ohm`: Ohms (0 if gas measurement invalid/disabled)

## Configuration (compile-time)

None. Oversampling, gas heater, and I2C address are runtime-configurable.

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Known issues

- Uses forced mode on every read (not continuous). For high-frequency sampling,
  consider normal mode (not yet implemented).
- BME680 also supports SPI — only I2C is implemented.
- Gas heater resistance calculation uses a simplified formula from Bosch
  reference code; accuracy may vary at extreme temperatures.

## How to test

```bash
cd wheels/drivers/bme680/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bme680
```

40 assertions across 20 test cases: init OK, chip ID verification, wrong chip
ID rejected, I2C fail on init, null safety (7 paths), calibration readback
(9 params), calibration I2C fail, temperature/humidity/pressure/gas read with
compensation, gas invalid returns 0, read I2C fail, oversampling register
encoding, oversampling out-of-range rejection, gas heater duration encoding,
gas heater I2C fail, reset command, reset I2C fail, alternate address (0x77).

## Test status

- Unit tests (sim): PASS (20 C tests, 40 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Context struct (`bme680_t`) holds all calibration data +
  I2C HAL pointer — fully reentrant for multi-sensor setups; Bosch integer
  compensation for MCU compatibility, converted to float at output; forced mode
  per-read to simplify state management; chip ID check on init (0xD0=0x61) to
  fail fast on wiring errors; delay_ms callback optional (NULL-safe) for
  measurement timing; gas heater separately configurable via set_gas_heater.

## Responsible human

@user
