# lis3dh — ST LIS3DH I2C 3-axis accelerometer driver

**Status: candidate**
**Layer: L1 (drivers)**

I2C driver for the ST LIS3DH 3-axis accelerometer. Supports configurable
full-scale range (+/-2/4/8/16g), output data rate (1 Hz to 5376 Hz),
and both scaled (G) and raw 16-bit reads. 12-bit resolution, left-justified.

## Why not VSF native?

No LIS3DH driver in `vsf/source/component/` or `vsf/source/service/`.

## Origin

Proactive build for common low-cost 3-axis accelerometer use case.

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
