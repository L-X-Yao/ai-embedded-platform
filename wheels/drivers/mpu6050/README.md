# mpu6050

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for InvenSense MPU-6050 6-axis IMU (3-axis
accelerometer + 3-axis gyroscope). I2C interface injected via callbacks.
Configurable full-scale ranges for both accel and gyro.
## Origin

Proactive build. MPU-6050 is the most widely used low-cost 6-axis IMU in
embedded applications (drones, robots, wearables, motion controllers).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

I2C HAL (injected via `mpu6050_i2c_t` callbacks). No other dependencies.

## API

```c
mpu6050_err_t mpu6050_init(mpu6050_t *dev, const mpu6050_i2c_t *i2c,
                           const mpu6050_cfg_t *cfg);
mpu6050_err_t mpu6050_read_accel(mpu6050_t *dev, mpu6050_accel_t *out);
mpu6050_err_t mpu6050_read_gyro(mpu6050_t *dev, mpu6050_gyro_t *out);
mpu6050_err_t mpu6050_reset(mpu6050_t *dev);
```

Output: `mpu6050_accel_t { float ax, ay, az; }` in g units,
`mpu6050_gyro_t { float gx, gy, gz; }` in dps.

## Configuration (compile-time)

None. Accel range (2/4/8/16 g), gyro range (250/500/1000/2000 dps), and
I2C address are runtime-configurable via `mpu6050_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Known issues

- No FIFO or DMP support — raw register reads only.
- No temperature readout (available in MPU-6050 but rarely used).
- Init does a device reset; caller must wait ~100ms after init before first read on real hardware.

## How to test

```bash
cd wheels/drivers/mpu6050/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mpu6050
```

29 assertions across 14 test cases: init OK, wrong WHO_AM_I, I2C fail on init,
null safety (8 paths), accel 1g on Z axis, negative accel, gyro 1 dps, 16G
accel range scaling, 2000 dps gyro range scaling, accel I2C fail, gyro I2C fail,
reset, alternate address (0x69), config register encoding verification.

## Test status

- Unit tests (sim): PASS (14 C tests, 29 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Separate accel/gyro read functions (not combined) to allow selective sampling; output in physical units (g, dps) not raw counts; device reset + PLL clock source set on init; WHO_AM_I check to fail fast on wiring errors; scale factors from datasheet sensitivity tables.

## Responsible human
@user
