# QMI8658A — 6-axis IMU Driver (L1)

Platform-agnostic driver for QST QMI8658A accelerometer + gyroscope.

## Why not VSF native?

VSF has no IMU/motion-sensor component. Searched `vsf/source/{component,service,hal}/` — zero hits for `qmi8658`, `imu`, `accel.*gyro`, `6axis`, `motion_sensor`.

## API

```c
qmi8658a_cfg_t  qmi8658a_default_cfg(void);
qmi8658a_err_t  qmi8658a_init(const qmi8658a_i2c_t *i2c, const qmi8658a_cfg_t *cfg);
qmi8658a_err_t  qmi8658a_read(const qmi8658a_i2c_t *i2c, qmi8658a_data_t *out);
qmi8658a_err_t  qmi8658a_reset(const qmi8658a_i2c_t *i2c);
```

Caller provides I2C HAL via `qmi8658a_i2c_t` callbacks. No RTOS dependency.

## Usage

```c
/* Platform-specific I2C implementation */
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

## Origin

Extracted from `projects/flybox/` QMI8658A driver. HIL verified on GD32F405 + QMI8658A @ I2C 400kHz.

## Test

```bash
cd test && mkdir build && cd build
cmake .. && make && ./test_qmi8658a
```
