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

## Datasheet check

Checked on 2026-11-06 against DPS310 datasheet V1.2, 2020-10-15, document reference IFX-sch1406115644540. Not run on hardware.

- **Matched:** Addresses, register map, MEAS_CFG and CFG_REG fields, oversampling codes and scale factors, soft reset value, coefficient layout and signs, compensation formulas, 24-bit result handling.
- **Fixed:** `dps310_init` read the coefficients without checking COEF_RDY, so on a cold boot it could store invalid calibration and return OK. It now polls COEF_RDY and SENSOR_RDY and returns the new `DPS310_ERR_TIMEOUT` if they never rise. The ID check tested REV_ID and ignored PROD_ID, so it refused other revisions and took other products. TMP_EXT was always set; it now follows TMP_COEF_SRCE, so a part calibrated against the ASIC sensor uses that sensor. The model now has MEAS_CFG at C0H, runs start-up through the delay callback, keeps the status bits on writes, clears the ready flags on reads and reruns start-up on a soft reset.
- **Still open:** Figure 4 shows no chip access for the first 2.5 ms after power-up, so `dps310_init` called that early fails with `DPS310_ERR_I2C`. No 128x oversampling, background mode, FIFO, interrupts or SPI. From memory and not in this manual: Infineon's Arduino library writes undocumented registers for parts that report about 60 C, so check for this on first hardware bring-up. The plain mock in `test_dps310.c` stores MEAS_CFG writes whole, which only affects the tests.
- **Suspected:** `dps310_reset` returns at once, and a read straight after it measures at 1x while still dividing by the configured kP and kT, so `dps310_init` should follow it. With `delay_ms` NULL only the bus paces the polls, about 48 ms at 400 kHz, so 32x and 64x measurements can time out, and so can the start-up wait on a faster bus. CFG_REG is written only when a shift is needed, so a second init at 8x or less keeps the old shift.

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
