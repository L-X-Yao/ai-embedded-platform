# dps310 -- Infineon DPS310 barometric pressure and temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the Infineon DPS310 barometric pressure and
temperature sensor. I2C interface injected via callbacks. Implements the
DPS310 compensation algorithm from the datasheet with float arithmetic.
Reads calibration coefficients on init and applies scaling factors per
oversampling setting.

## Origin

Proactive build. DPS310 is a popular high-precision barometric sensor
used in drones, indoor navigation, and weather monitoring.

## Why self-written?

No existing open-source component for DPS310. Self-written for zero-dependency I2C HAL
injection and platform convention consistency.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `dps310_i2c_t` callbacks). No other dependencies.

## API

```c
dps310_err_t dps310_init(dps310_t *dev, const dps310_i2c_t *i2c,
                         const dps310_cfg_t *cfg);
dps310_err_t dps310_read(dps310_t *dev, dps310_data_t *out);
dps310_err_t dps310_reset(dps310_t *dev);
```

Output: `dps310_data_t { float temperature_c; float pressure_pa; }`

## Usage

```c
dps310_t sensor;
dps310_cfg_t cfg = { .addr = DPS310_ADDR_HIGH,
                     .osrs_t = DPS310_OVERSAMPLING_4,
                     .osrs_p = DPS310_OVERSAMPLING_16 };
dps310_init(&sensor, &i2c_hal, &cfg);

dps310_data_t data;
dps310_read(&sensor, &data);
// data.temperature_c, data.pressure_pa
```

## Configuration (compile-time)

None. Oversampling and I2C address are runtime-configurable via
`dps310_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- I2C only; SPI not implemented.

## How to test

```bash
cd wheels/drivers/dps310/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_dps310
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

@user
