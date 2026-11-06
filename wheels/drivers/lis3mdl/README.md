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

## Datasheet check

Checked on 2026-11-06 against ST DocID024204 Rev 6, May 2017. Not run on hardware. The reference above cites DocID025866 Rev 3, which needs confirming.

- **Matched:** Addresses, WHO_AM_I, CTRL_REG1 to CTRL_REG3 addresses and values, full-scale codes, sensitivities and the conversion to microtesla, the 0xA8 burst read, the output order and format.
- **Fixed:** The model stepped the address without SUB bit 7, started CTRL_REG1 and CTRL_REG3 at 0x00, had no STATUS_REG and dropped REBOOT, SOFT_RST, LP and SIM. It now has the manual's reset values and masks, and samples come from a clock, so tests wait for one.
- **Still open:** ODR and performance mode are fixed at 10 Hz and medium. Temperature output, self-test, soft reset, reboot, interrupts, overrun reporting and SPI are not implemented.
- **Suspected:** `lis3mdl_read_raw` never reads STATUS_REG, so a read in the first 100 ms after init, or polling faster than 10 Hz, returns old data with `LIS3MDL_OK`. BDU stays 0, so a sample landing mid-read can pair bytes from two samples. CTRL_REG4 is never written, so the Z axis stays in low-power mode while X and Y run at medium performance.

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
