# bmi160 — Bosch BMI160 6-axis inertial measurement unit

**Status: candidate**
**Layer: L1 (drivers)**

I2C driver for the Bosch BMI160 6-axis IMU (3-axis accelerometer + 3-axis gyroscope).
Outputs acceleration in milli-g and angular rate in degrees per second.
Configurable full-scale ranges for both accel and gyro.

## Origin

Proactive build for IMU-based projects (motion tracking, stabilization, orientation).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- boschsensortec/BMI160_driver: full-featured but heavy, pulls in platform abstraction layer, not zero-dependency C99.
- No suitable zero-dependency C99 implementation found matching our wheel contract.

Reference: Bosch BMI160 datasheet (BST-BMI160-DS000, Rev 1.4).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
bmi160_err_t bmi160_init(bmi160_t *dev, const bmi160_i2c_t *i2c,
                         const bmi160_cfg_t *cfg);
bmi160_err_t bmi160_read(const bmi160_t *dev, bmi160_data_t *out);
bmi160_err_t bmi160_reset(bmi160_t *dev);
```

## Configuration (compile-time)

| Define            | Default | Purpose                     |
|-------------------|---------|-----------------------------|
| CONTRACT_ENABLED  | off     | Enable runtime contract checks |

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/bmi160/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bmi160
./build/test_hw_model_bmi160
```

## Test status

- Unit tests (sim): 22 mock tests + 14 hw_model tests = 36 total, all pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: accel output in mg (not G) per user spec; gyro output in dps; range enums use datasheet register values for direct write

## Responsible human

@user
