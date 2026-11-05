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

## Datasheet check

Checked on 2026-11-06 against BST-BMI160-DS000-09, Revision 1.0, November 2020. Not run on hardware.

- **Matched:** I2C addresses, chip ID, register addresses, the power and reset commands, range codes, data byte order, the 80 ms gyro wait, the model's reset values.
- **Fixed:** With `delay_ms` NULL the gyro command reached CMD while the accel command still held it, the chip dropped it, and `bmi160_init` returned OK with the gyro in suspend, so `delay_ms` is now required. The sensitivities were rounded, so 1 g read 999.42 mg and 100 dps read 100.11 dps; they now use the manual's LSB/g and LSB/dps figures. The model now holds CMD for the Table 24 time and drops writes meanwhile.
- **Still open:** `bmi160_read` never reads STATUS, so a caller cannot tell a new sample from an old one. PMU_STATUS and ERR_REG are not read, so `bmi160_init` cannot report a dropped command. ODR and filter stay at the 100 Hz default. The caller must allow the power-up of under 10 ms before `bmi160_init`. No SPI, FIFO, interrupts, temperature, magnetometer interface, fast offset compensation or self-test. The model fills the data registers whatever PMU_STATUS says, so a sensor left in suspend cannot show in the tests.
- **Suspected:** The 4 ms accel wait is under the 4.1 ms worst case from all-suspend, which could drop the gyro command. `bmi160_reset` returns without waiting and keeps the cached sensitivities. Gyro and accel come from two separate reads, so one result can mix two updates. The ranges are written after the sensors start, so a read within one sample period can scale an old sample by the new range.

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

- Unit tests (sim): 23 mock tests + 18 hw_model tests = 41 total, all pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: accel output in mg (not G) per user spec; gyro output in dps; range enums use datasheet register values for direct write

## Responsible human

@user
