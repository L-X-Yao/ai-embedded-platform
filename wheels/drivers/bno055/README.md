# bno055

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Bosch BNO055 9-axis absolute orientation IMU
with on-chip sensor fusion. I2C interface injected via callbacks. Outputs
Euler angles (heading/roll/pitch) from the built-in NDOF fusion algorithm.
Scale: 1/16 degree per LSB.

## Origin

Proactive build. BNO055 is the most popular sensor-fusion IMU for robotics,
drones, and VR applications (built-in ARM Cortex-M0 runs fusion on-chip).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Bosch official driver (BNO055_driver) has complex multi-platform abstraction.
- Adafruit library is Arduino-specific.
- No suitable zero-dependency C99 implementation found matching wheel contract.

Reference: Bosch BNO055 datasheet (BST-BNO055-DS000, Rev. 1.8).

## Dependencies

I2C HAL (injected via `bno055_i2c_t` callbacks). delay_ms required for mode
switch timing. No other dependencies.

## API

```c
bno055_err_t bno055_init(bno055_t *dev, const bno055_i2c_t *i2c,
                         const bno055_cfg_t *cfg);
bno055_err_t bno055_read_euler(bno055_t *dev, bno055_euler_t *out);
bno055_err_t bno055_read_calib_status(bno055_t *dev,
                                      bno055_calib_status_t *out);
bno055_err_t bno055_set_mode(bno055_t *dev, bno055_opmode_t mode);
```

Output: `bno055_euler_t { float heading_deg; float roll_deg; float pitch_deg; }`

## Configuration (compile-time)

None. Operation mode and I2C address are runtime-configurable via `bno055_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Mode switch requires up to 600ms delay (fusion modes).
- Only Euler angles exposed; quaternion and linear acceleration available in
  registers but not yet wrapped.
- Calibration save/restore not implemented (registers 0x55-0x6A).

## How to test

```bash
cd wheels/drivers/bno055/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bno055
```

31 assertions across 15 test cases + 8 hardware model tests + 8 null tests.

## Test status

- Unit tests (sim): PASS (15 C tests, 8 hw_model tests, 8 null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: Init does full reset + mode switch; Euler API separate from quaternion (simpler, most common use case); calib_status for runtime monitoring; delay_ms required (not optional) for correct behavior.

## Responsible human

@user
