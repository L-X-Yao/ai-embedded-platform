# lis3mdl — ST LIS3MDL 3-axis digital magnetometer driver

**Status: candidate**
**Layer: L1 (drivers)**

I2C-based driver for the ST LIS3MDL 3-axis magnetometer. Outputs magnetic field in microtesla (uT). Supports four full-scale ranges (+/-4, +/-8, +/-12, +/-16 gauss) and configurable I2C address (0x1C or 0x1E via SA1 pin).

## Origin

Proactive build for magnetometer use cases (compass heading, geomagnetic sensing). Common companion to IMU sensors in 9-DOF fusion setups.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- ST official driver (STMicroelectronics/lis3mdl-pid): HAL-dependent, not zero-dependency C99.
- No suitable zero-dependency C99 implementation found that matches wheel contract.

Reference: ST LIS3MDL datasheet (DocID025866 Rev 3).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
/* Error codes */
typedef enum {
    LIS3MDL_OK        =  0,
    LIS3MDL_ERR_I2C   = -1,
    LIS3MDL_ERR_ID    = -2,
    LIS3MDL_ERR_PARAM = -3,
} lis3mdl_err_t;

/* Configure and verify WHO_AM_I. Sets range + continuous mode. */
lis3mdl_err_t lis3mdl_init(lis3mdl_t *dev, const lis3mdl_i2c_t *i2c,
                           const lis3mdl_cfg_t *cfg);

/* Read magnetic field in microtesla. */
lis3mdl_err_t lis3mdl_read(lis3mdl_t *dev, lis3mdl_data_t *out);

/* Read raw 16-bit signed values. */
lis3mdl_err_t lis3mdl_read_raw(const lis3mdl_t *dev, lis3mdl_raw_t *raw);
```

## Configuration (compile-time)

| Define             | Default | Purpose                          |
|--------------------|---------|----------------------------------|
| CONTRACT_ENABLED   | off     | Enable ENSURE NaN guard in read  |

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/lis3mdl/test
cmake -GNinja -S . -B build_ta && cmake --build build_ta
ctest --test-dir build_ta --output-on-failure
```

## Test status

- Unit tests (sim): 47 tests (22 main + 7 null + 2 prop + 16 hw_model), all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: Output in microtesla (not gauss) per user spec. Auto-increment bit 7 for multi-byte reads per datasheet. Config struct includes I2C address for dual-device support.

## Responsible human

@user
