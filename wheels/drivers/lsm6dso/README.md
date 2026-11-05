# lsm6dso -- ST LSM6DSO 6-axis accelerometer and gyroscope driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the ST LSM6DSO 6-axis IMU (accelerometer +
gyroscope). I2C interface injected via callbacks. Configurable full-scale
ranges and output data rates for both sensors. Includes die temperature
reading and software reset. Output in millig (accel) and deg/s (gyro).

## Origin

Proactive build. LSM6DSO is a popular automotive-grade 6-axis IMU used
in robotics, drones, and industrial motion tracking.

## Why self-written?

No existing open-source component for LSM6DSO. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `lsm6dso_i2c_t` callbacks). No other dependencies.

## API

```c
lsm6dso_err_t lsm6dso_init(lsm6dso_t *dev, const lsm6dso_i2c_t *i2c,
                            const lsm6dso_cfg_t *cfg);
lsm6dso_err_t lsm6dso_read_accel(const lsm6dso_t *dev,
                                  lsm6dso_accel_data_t *out);
lsm6dso_err_t lsm6dso_read_gyro(const lsm6dso_t *dev,
                                 lsm6dso_gyro_data_t *out);
lsm6dso_err_t lsm6dso_read_temp(const lsm6dso_t *dev, float *temperature_c);
lsm6dso_err_t lsm6dso_reset(lsm6dso_t *dev);
```

## Usage

```c
lsm6dso_t imu;
lsm6dso_cfg_t cfg = { .addr = LSM6DSO_ADDR_HIGH,
                      .accel_range = LSM6DSO_ACCEL_4G,
                      .gyro_range = LSM6DSO_GYRO_500DPS,
                      .odr = LSM6DSO_ODR_104HZ };
lsm6dso_init(&imu, &i2c_hal, &cfg);

lsm6dso_accel_data_t accel;
lsm6dso_read_accel(&imu, &accel);
// accel.x_mg, accel.y_mg, accel.z_mg
```

## Configuration (compile-time)

None. Ranges, ODR, and I2C address are runtime-configurable via
`lsm6dso_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against ST DS12140 Rev 2. Not run on hardware.

- **Matched:** Addresses, WHO_AM_I, register map, ODR and full-scale codes, sensitivities.
- **Fixed:** init cleared IF_INC when it set BDU, so every multi-byte read stayed on its first register and each axis came from one low byte. The model and mock now start from the reset value, and the model honours IF_INC, which is why this now fails if it comes back.
- **Still open:** The model has no STATUS_REG behaviour, and `test_reset` expects SW_RESET to stay set, where the chip clears it.
- **Suspected:** init and `lsm6dso_reset` do not wait for SW_RESET to finish. Reads never check STATUS_REG, so a read within the 35 ms turn-on time returns old data. I3C_disable is never set.

## Known issues

- I2C only; SPI not implemented.

## How to test

```bash
cd wheels/drivers/lsm6dso/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lsm6dso
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
