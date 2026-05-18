# lis2dh12 -- ST LIS2DH12 3-axis accelerometer driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the ST LIS2DH12 ultra-low-power 3-axis
accelerometer. I2C interface injected via callbacks. 10-bit normal mode
with configurable full-scale range (2/4/8/16G) and output data rate.
Output in milligravity (mg).

## Origin

Proactive build. LIS2DH12 is one of the most popular ultra-low-power
accelerometers for wearables, IoT, and motion detection applications.

## Why not VSF native?

No VSF component for LIS2DH12. Self-written for zero-dependency I2C HAL
injection and platform conventions.

## Dependencies

I2C HAL (injected via `lis2dh12_i2c_t` callbacks). No other dependencies.

## API

```c
lis2dh12_err_t lis2dh12_init(lis2dh12_t *dev, const lis2dh12_i2c_t *i2c,
                              const lis2dh12_cfg_t *cfg);
lis2dh12_err_t lis2dh12_read(lis2dh12_t *dev, lis2dh12_data_t *out);
```

Output: `lis2dh12_data_t { float x_mg; float y_mg; float z_mg; }`

## Usage

```c
lis2dh12_t accel;
lis2dh12_cfg_t cfg = { .addr = LIS2DH12_ADDR_LOW,
                       .range = LIS2DH12_RANGE_2G,
                       .odr = LIS2DH12_ODR_100HZ };
lis2dh12_init(&accel, &i2c_hal, &cfg);

lis2dh12_data_t data;
lis2dh12_read(&accel, &data);
// data.x_mg, data.y_mg, data.z_mg
```

## Configuration (compile-time)

None. Range, ODR, and I2C address are runtime-configurable via
`lis2dh12_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Normal mode (10-bit) only; high-resolution (12-bit) and low-power
  (8-bit) modes are not implemented.

## How to test

```bash
cd wheels/drivers/lis2dh12/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lis2dh12
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## Responsible human

@user
