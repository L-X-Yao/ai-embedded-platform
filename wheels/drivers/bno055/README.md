# bno055

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Bosch BNO055 9-axis absolute orientation
IMU with on-chip sensor fusion. I2C interface injected via callbacks.
Reads Euler angles (heading, roll, pitch) and calibration status.
Supports all 13 operation modes including NDOF full sensor fusion.

## Origin

Proactive build (ADR-011). BNO055 is the most popular turnkey 9-axis
IMU in robotics and drones due to its built-in sensor fusion processor.

## Why self-written?

Searched GitHub (2026-05-11). No single-file zero-dependency C99 driver
matching our wheel contract. Self-written; register map and timing from
BST-BNO055-DS000 datasheet.

## Dependencies

I2C HAL (injected via `bno055_i2c_t` callbacks). No other dependencies.

## API

```c
bno055_err_t bno055_init(bno055_t *dev, const bno055_i2c_t *i2c,
                         const bno055_cfg_t *cfg);
bno055_err_t bno055_read_euler(const bno055_t *dev, bno055_euler_t *out);
bno055_err_t bno055_get_calib_status(const bno055_t *dev,
                                     bno055_calib_status_t *out);
bno055_err_t bno055_set_mode(bno055_t *dev, bno055_opr_mode_t mode);
```

Output: `bno055_euler_t { float heading_deg; float roll_deg; float pitch_deg; }`

## Configuration (compile-time)

None. Operation mode and I2C address are runtime-configurable via `bno055_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Only Euler angles are read; quaternion, linear acceleration, and
  gravity vector outputs are not yet implemented.
- Calibration save/restore not implemented.
- Mode switching goes through CONFIG mode every time (correct per
  datasheet but adds latency).

## How to test

```bash
cd wheels/drivers/bno055/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bno055
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## API Review

Pending.

## Responsible human
@user
