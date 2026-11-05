# bmi270 -- Bosch BMI270 SPI 6-axis IMU driver

**Status: candidate**
**Layer: L1 (drivers)**

> **Not usable on hardware yet.** `bmi270_init` skips the 8 KB config file upload the chip requires, so on a real part it stops at `BMI270_ERR_CONFIG`.

Platform-agnostic driver for the Bosch BMI270 6-axis inertial measurement
unit (accelerometer + gyroscope). SPI interface injected via callbacks.
Handles config file loading, chip ID verification, and per-axis scaling
to G and deg/s units. Configurable ranges and output data rates.

## Origin

Proactive build. BMI270 is a widely used low-power IMU for drones,
wearables, and robotics requiring 6-axis motion sensing.

## Why self-written?

No existing open-source component for BMI270. Self-written for zero-dependency SPI HAL
injection and platform convention consistency.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

SPI HAL (injected via `bmi270_spi_t` callbacks). No other dependencies.

## API

```c
bmi270_cfg_t bmi270_default_cfg(void);
bmi270_err_t bmi270_init(bmi270_t *dev, const bmi270_spi_t *spi,
                         const bmi270_cfg_t *cfg);
bmi270_err_t bmi270_read_accel_gyro(const bmi270_t *dev,
                                    bmi270_accel_gyro_t *out);
bmi270_err_t bmi270_read_temp(const bmi270_t *dev, float *temp_c);
bmi270_err_t bmi270_set_acc_odr(const bmi270_t *dev, bmi270_odr_t odr);
bmi270_err_t bmi270_set_gyro_odr(const bmi270_t *dev, bmi270_odr_t odr);
bmi270_err_t bmi270_set_acc_range(bmi270_t *dev, bmi270_acc_range_t range);
bmi270_err_t bmi270_set_gyro_range(bmi270_t *dev, bmi270_gyro_range_t range);
bmi270_err_t bmi270_soft_reset(const bmi270_t *dev);
```

Output: `bmi270_accel_gyro_t { float ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps; }`

## Usage

```c
bmi270_t imu;
bmi270_cfg_t cfg = bmi270_default_cfg();
bmi270_init(&imu, &spi_hal, &cfg);

bmi270_accel_gyro_t data;
bmi270_read_accel_gyro(&imu, &data);
// data.ax_g, data.gx_dps, etc.
```

## Configuration (compile-time)

None. Ranges and ODR are runtime-configurable via `bmi270_cfg_t` and
setter functions.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected SPI HAL)

## Datasheet check

Checked on 2026-11-05 against BST-BMI270-DS000-08 and Bosch `bmi2_defs.h`. Not run on hardware.

- **Matched:** 27 register addresses, chip ID, soft reset, range and ODR codes, SPI framing, data layout, accel and temperature scaling.
- **Fixed:** PWR_CTRL turned on aux instead of the gyro; ACC_CONF and GYR_CONF cleared the filter bit, so neither ran in normal mode; missing 450 us and 200 us waits; INTERNAL_STATUS read as one bit, which passed error codes; gyro sensitivity constants; `delay_ms` is now required; soft reset waits for the chip; reads after a failed init are refused.
- **Still open:** Config file upload. GYR_CAS correction on the X axis, which needs the config file first.

## Known issues

- SPI only; I2C not implemented.
- The 8 KB config file is not uploaded, see above.

## How to test

```bash
cd wheels/drivers/bmi270/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bmi270
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
