# QMI8658A — 6-axis IMU Driver

**STATUS: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for QST QMI8658A accelerometer + gyroscope.

## Why not VSF native?

VSF has no IMU/motion-sensor component. Searched `vsf/source/{component,service,hal}/` — zero hits for `qmi8658`, `imu`, `accel.*gyro`, `6axis`, `motion_sensor`.

## Features

- WHOAMI register check (0x05)
- Configurable accelerometer range (2/4/8/16 G)
- Configurable gyroscope range (16 to 2048 deg/s)
- Configurable output data rate (31 Hz to 8 kHz)
- Acceleration output in G, angular rate in deg/s (float)
- Soft reset command
- I2C HAL injection via callback struct — zero RTOS dependency

## API

```c
qmi8658a_cfg_t  qmi8658a_default_cfg(void);
qmi8658a_err_t  qmi8658a_init(const qmi8658a_i2c_t *i2c, const qmi8658a_cfg_t *cfg);
qmi8658a_err_t  qmi8658a_read(const qmi8658a_i2c_t *i2c, qmi8658a_data_t *out);
qmi8658a_err_t  qmi8658a_reset(const qmi8658a_i2c_t *i2c);
```

Caller provides I2C HAL via `qmi8658a_i2c_t` callbacks (write_reg + read_regs).

## Usage

```c
static bool my_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val) { ... }
static bool my_read_regs(void *ctx, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) { ... }

qmi8658a_i2c_t i2c = { .write_reg = my_write_reg, .read_regs = my_read_regs, .ctx = NULL };
qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
cfg.acc_range = QMI8658A_ACC_RANGE_8G;

if (qmi8658a_init(&i2c, &cfg) == QMI8658A_OK) {
    qmi8658a_data_t data;
    qmi8658a_read(&i2c, &data);
    /* data.ax/ay/az in G, data.gx/gy/gz in deg/s */
}
```

## Supported platforms

- [x] Host C99 (mock I2C) — 19 tests, 44 assertions, 2026-04-29
- [x] GD32F405 + QMI8658A @ I2C1 400 kHz (PB10/PB11) — HIL verified 2026-04-26

## Dependencies

None. Pure C99 with injected I2C HAL callbacks.

## Known issues

- CTRL2/CTRL3 register values are hard-coded to match QMI8658A datasheet defaults for common configs; exotic ODR/range combos should be validated against datasheet Table 19/20
- Flybox consumer renames public symbols to `imu_init`/`imu_read_data`/`imu_raw_t` to avoid collision with wheel's `qmi8658a_*` namespace (flybox `imu_task.h` already uses `imu_data_t` for fused attitude output)

## Test status

- Host mock tests: PASS (19 tests, 44 assertions — init, chip ID validation, config register writes, mid-init I2C failure at CTRL2/CTRL3/CTRL7, data read conversion, all 4 acc ranges, gyro range scaling, range/ODR combinations, I2C failure propagation, boundary values, NULL safety, re-init config update, reset I2C failure, all-six-axes read)
- HIL: PASS on GD32F405 + QMI8658A (I2C1 PB10/PB11, 400 kHz), 2026-04-26

## API convention deviations

None.

## Responsible human

@user

## Origin

Extracted from `projects/flybox/src/drivers/imu/imu_driver.c`.

## API review

- Date: 2026-04-25
- Reviewer: @user
- Key decisions: expose `default_cfg()` for common defaults; output in physical units (G / deg/s) not raw counts; separate `reset()` from `init()`

## Upgrade to stable

- [ ] 2nd project consumer (currently only flybox)
- [ ] Performance reference (read latency, I2C transaction count per read)
- [ ] Change history
