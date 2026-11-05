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

## Datasheet check

Checked on 2026-11-05 against BST-BNO055-DS000-18 and Bosch `bno055.h`. Not run on hardware.

- **Matched:** Register addresses, chip ID, bit fields, 15 mode values, mode switch delays, Euler data layout and scale.
- **Fixed:** `bno055_read_euler` returned zeros as success outside the fusion modes; `delay_ms` is now required; the header now states the 650 ms start-up time and which COM3 level selects each address.
- **Still open:** `BNO055_ADDR_PRIMARY` is 0x28 although the datasheet calls 0x29 the default; the name is kept to avoid an API change. PAGE_ID and UNIT_SEL are assumed to be at their power-on values.

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

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
