# QMI8658A — 6-axis IMU Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for QST QMI8658A accelerometer + gyroscope.

## Origin

Reimplemented for the needs of an internal project.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. Niche QST chip; no pure C driver exists. Self-written with HAL injection per platform conventions.

## Dependencies

None. Pure C99 with injected I2C HAL callbacks.

## API

```c
qmi8658a_cfg_t  qmi8658a_default_cfg(void);
qmi8658a_err_t  qmi8658a_init(qmi8658a_t *dev, const qmi8658a_i2c_t *i2c,
                               const qmi8658a_cfg_t *cfg);
qmi8658a_err_t  qmi8658a_read(const qmi8658a_t *dev, qmi8658a_data_t *out);
qmi8658a_err_t  qmi8658a_reset(const qmi8658a_t *dev);
```

Caller provides I2C HAL via `qmi8658a_i2c_t` callbacks (write_reg + read_regs).

## Usage

```c
static bool my_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
    { (void)ctx; (void)addr; (void)reg; (void)val; return true; }
static bool my_read_regs(void *ctx, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
    { (void)ctx; (void)addr; (void)reg; (void)buf; (void)len; return true; }

static void example(void) {
    qmi8658a_i2c_t i2c = { .write_reg = my_write_reg, .read_regs = my_read_regs, .ctx = NULL };
    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    cfg.acc_range = QMI8658A_ACC_RANGE_8G;

    if (qmi8658a_init(&dev, &i2c, &cfg) == QMI8658A_OK) {
        qmi8658a_data_t data;
        qmi8658a_read(&dev, &data);
        /* data.ax_g/ay_g/az_g in G, data.gx_dps/gy_dps/gz_dps in deg/s */
    }
}
```

## Supported platforms

- [x] Host C99 (mock I2C) — 20 tests, 2026-04-30
- [x] GD32F405 + QMI8658A @ I2C1 400 kHz (PB10/PB11) — HIL verified 2026-04-26 (the code as of that date; later changes have not run on a board)

## Datasheet check

Checked on 2026-11-06 against the QST QMI8658A Datasheet Rev A, document 13-52-25, June 20, 2022. This check was not run on hardware. The driver was not changed: the owner decided it is checked but not fixed, because no board is available to retest it.

- **Matched:** WHO_AM_I 0x05, address 0x6B for SA0 low, CTRL1 = 0x40, CTRL2 and CTRL3 fields and encodings, CTRL7 = 0x03, the 12-byte little-endian data block from 0x35, sensitivities, RESET register 0x60.
- **Fixed:** The model reset CTRL1 to 0 instead of 0x20, ignored ADDR_AI and BE, and gave data with the sensors off. It now follows the manual on these, so a wrong CTRL1 or CTRL7 fails the tests. Sensors give data at once there: the turn-on time and the 15 ms of a reset are not modelled.
- **Still open:** `qmi8658a_reset` returns at once although the reset takes up to 15 ms, so an init straight after it writes during the reset. The reset also turns the sensors off and clears ADDR_AI, yet `qmi8658a_read` keeps returning OK with no live data until init runs again. Register 0x4D is never read, so a failed reset goes unnoticed. flybox never calls reset, so its board run says nothing about these. Reads are not synchronised or locked, so a sample update during the burst can mix two samples, which shows as rare spikes. The driver always runs both sensors, and in that mode the ODR codes give 7174.4 down to 28.025 Hz, about 10% below the enum names: `QMI8658A_ODR_500` is really 448.4 Hz, so a dt taken from the name is about 10% off. The address is fixed at 0x6B, so a board with SA0 high or floating, which gives 0x6A, is not found.
- **Suspected:** Section 7.4 gives the reset value as 0x0B where Table 27 gives the 0xB0 the driver writes, probably a typo in the manual, but no hardware run settles it. Init writes straight after WHO_AM_I, which breaks the 15 ms start-up rule if it runs right after power-up. `qmi8658a_read` never checks STATUS0, so for about 150 ms plus 3/ODR after init it may return zeros or unsettled data with OK.

## Known issues

- CTRL2/CTRL3 register values are hard-coded to match QMI8658A datasheet defaults for common configs; exotic ODR/range combos should be validated against datasheet Table 19/20
- Flybox consumer renames public symbols to `imu_init`/`imu_read_data`/`imu_raw_t` to avoid collision with wheel's `qmi8658a_*` namespace (flybox `imu_task.h` already uses `imu_data_t` for fused attitude output)

## How to test

```bash
cd wheels/drivers/qmi8658a/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Host mock tests: PASS (20 tests — init, chip ID validation, config register writes, mid-init I2C failure at CTRL2/CTRL3/CTRL7, data read conversion, all 4 acc ranges, gyro range scaling, range/ODR combinations, I2C failure propagation, boundary values, NULL safety, re-init config update, reset I2C failure, all-six-axes read, multi-instance isolation)
- HIL: PASS on GD32F405 + QMI8658A (I2C1 PB10/PB11, 400 kHz), 2026-04-26

## API convention deviations

None.

## Upgrade to stable

- [ ] 2nd project consumer (currently only flybox)
- [ ] Performance reference (read latency, I2C transaction count per read)
- [ ] Change history

## Features

- WHOAMI register check (0x05)
- Configurable accelerometer range (2/4/8/16 G)
- Configurable gyroscope range (16 to 2048 deg/s)
- Configurable output data rate (31 Hz to 8 kHz)
- Acceleration output in G, angular rate in deg/s (float)
- Soft reset command
- I2C HAL injection via callback struct — zero RTOS dependency

## API review

- Date: 2026-04-25
- Reviewer: @user
- Key decisions: expose `default_cfg()` for common defaults; output in physical units (G / deg/s) not raw counts; separate `reset()` from `init()`

## Responsible human

@user
