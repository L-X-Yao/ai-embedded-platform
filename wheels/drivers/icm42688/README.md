# icm42688 — TDK ICM-42688-P 6-axis IMU driver (SPI)

**Status: candidate**
**Layer: L1 (drivers)**

SPI driver for the TDK ICM-42688-P high-performance 6-axis MEMS MotionTracking device.
Supports 3-axis accelerometer (2/4/8/16g), 3-axis gyroscope (250/500/1000/2000 dps),
die temperature reading, configurable ODR (1 Hz to 32 kHz), and power mode control.

## Origin

Proactive build for general-purpose IMU applications (drones, robotics, motion tracking).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- SparkFun ICM-42688 library: Arduino/C++ only, heavy STL dependency.
- InvenSense SDK: vendor HAL with platform-specific bindings.
- No suitable zero-dependency C99 SPI-only implementation found.

Reference: TDK ICM-42688-P Datasheet (DS-000347 Rev. 1.9).

## Dependencies

None (SPI HAL injected via function pointers).

## API

```c
icm42688_cfg_t   icm42688_default_cfg(void);
icm42688_err_t   icm42688_init(icm42688_t *dev, const icm42688_spi_t *spi,
                                const icm42688_cfg_t *cfg);
icm42688_err_t   icm42688_read(const icm42688_t *dev, icm42688_data_t *out);
icm42688_err_t   icm42688_read_temperature(const icm42688_t *dev, float *temp_c);
icm42688_err_t   icm42688_set_power_mode(const icm42688_t *dev,
                                          icm42688_power_mode_t accel_mode,
                                          icm42688_power_mode_t gyro_mode);
icm42688_err_t   icm42688_soft_reset(const icm42688_t *dev);
```

## Configuration (compile-time)

No compile-time defines needed. All configuration via `icm42688_cfg_t` at runtime.

## Supported platforms

- [x] Host (sim) -- 2026-05-06
- [ ] Real hardware target -- not yet verified

## Datasheet check

Not checked: the TDK datasheet could not be downloaded on 2026-11-06, so registers, timing and scaling here are unverified.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/icm42688/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_icm42688
```

## Test status

- Unit tests (sim): 32 tests, all passing
- Null param tests: 9 auto-generated, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: SPI-only (no I2C support); burst read for temp+accel+gyro in single transaction; power mode enum shared between accel and gyro with per-axis semantics matching datasheet PWR_MGMT0 register layout.

## Responsible human

@user
