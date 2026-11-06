# lis3dh — ST LIS3DH I2C 3-axis accelerometer driver

**Status: candidate**
**Layer: L1 (drivers)**

I2C driver for the ST LIS3DH 3-axis accelerometer. Supports configurable
full-scale range (+/-2/4/8/16g), output data rate (1 Hz to 5376 Hz),
and both scaled (G) and raw 16-bit reads. 12-bit resolution, left-justified.
## Origin

Proactive build for common low-cost 3-axis accelerometer use case.


## Why self-written?

Searched GitHub (2026-05-02). Referenced STMicroelectronics/lis3dh-pid (BSD-3) for register sequences and compensation algorithms. Official ST driver; register sequences verified against. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

- `i2c_hal` (injected via callbacks, no link dependency)

## API

```c
lis3dh_err_t lis3dh_init(lis3dh_t *dev, const lis3dh_i2c_t *i2c,
                         const lis3dh_cfg_t *cfg);
lis3dh_err_t lis3dh_read_accel(const lis3dh_t *dev, lis3dh_accel_t *out);
lis3dh_err_t lis3dh_read_raw(const lis3dh_t *dev, lis3dh_raw_t *out);
lis3dh_err_t lis3dh_set_range(lis3dh_t *dev, lis3dh_range_t range);
lis3dh_err_t lis3dh_set_odr(lis3dh_t *dev, lis3dh_odr_t odr);
```

## Configuration (compile-time)

None. All configuration via `lis3dh_cfg_t` at init or runtime setters.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against ST DocID17530 Rev 2, and Rev 1 of May 2010. Not run on hardware.

- **Matched:** Addresses, WHO_AM_I, register map, ODR and range codes, auto-increment, the scale table, which holds in all three modes.
- **Fixed:** CTRL_REG4 left HR at 0, so the chip ran 10-bit normal mode where the driver states 12-bit. LPen was never set, so the `_LP` rates did not run as named. LPen and HR now follow the rate, and are never set together. The model and mocks now treat bit 7 of the sub-address as auto-increment instead of address, and the model resets CTRL_REG1 to 0x07, ignores writes to type r registers and counts writes to reserved ones.
- **Suspected:** init and `lis3dh_set_odr` do not wait for the turn-on time, and reads do not check ZYXDA. `lis3dh_set_range` changes the scale at once, so the first read after it can apply the new scale to an old sample.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/lis3dh/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lis3dh
```

## Test status

- Unit tests (sim): 20 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Follows mpu6050 pattern (cfg struct, injected I2C, scale divisor). Added runtime set_range/set_odr and read_raw per task spec.

## Responsible human

@user
