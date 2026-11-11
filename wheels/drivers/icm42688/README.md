# icm42688 — TDK ICM-42688-P 6-axis IMU driver (SPI)

**Status: candidate**
**Layer: L1 (drivers)**

SPI driver for the TDK ICM-42688-P high-performance 6-axis MEMS MotionTracking device.
Supports 3-axis accelerometer (2/4/8/16g), 3-axis gyroscope (250/500/1000/2000 dps),
die temperature reading, configurable ODR (12.5 Hz to 32 kHz), and power mode control.

## Origin

Proactive build for general-purpose IMU applications (drones, robotics, motion tracking).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- SparkFun ICM-42688 library: Arduino/C++ only, heavy STL dependency.
- InvenSense SDK: vendor HAL with platform-specific bindings.
- No suitable zero-dependency C99 SPI-only implementation found.

Reference: TDK ICM-42688-P Datasheet (DS-000347 Rev. 1.9).

## Dependencies

None (SPI HAL injected via function pointers).

## API

```c
icm42688_cfg_t   icm42688_default_cfg(void);
icm42688_err_t   icm42688_init(icm42688_t *dev, const icm42688_spi_t *spi,
                                const icm42688_cfg_t *cfg);
icm42688_err_t   icm42688_read(const icm42688_t *dev, icm42688_data_t *out);
icm42688_err_t   icm42688_read_temperature(const icm42688_t *dev, float *temp_c);
icm42688_err_t   icm42688_set_power_mode(const icm42688_t *dev,
                                          icm42688_power_mode_t accel_mode,
                                          icm42688_power_mode_t gyro_mode);
icm42688_err_t   icm42688_soft_reset(icm42688_t *dev);
```

## Configuration (compile-time)

No compile-time defines needed. All configuration via `icm42688_cfg_t` at runtime.

## Supported platforms

- [x] Host (sim) -- 2026-05-06
- [ ] Real hardware target -- not yet verified

## Datasheet check

Checked on 2026-11-06 against TDK DS-000347 Revision 1.6, 06/20/2021, a copy of TDK's document from a third-party site, not the Rev. 1.9 named above. Not run on hardware.

- **Matched:** WHO_AM_I 0x47, bank 0 register addresses, SPI framing, the 14-byte big-endian burst from 0x1D, FS_SEL and ODR encodings, sensitivities, the temperature formula, PWR_MGMT0 mode bits, the soft reset bit.
- **Fixed:** `icm42688_init` accepted gyro ODR codes the manual reserves, and accel rates below 12.5 Hz, which low-noise mode does not support. `icm42688_set_power_mode` accepted gyro LP, which is reserved, accel standby, which only turns the accel off, and accel LP above 500 Hz. `icm42688_read` and `icm42688_read_temperature` returned OK with about -8 g, -2000 dps and -222 C before the first sample, and now return `ICM42688_ERR_NOT_READY`; the mock and hw_model reset the data registers to 0, which hid this. Comments gave FS_SEL as bits [6:5] instead of [7:5]. The hw_model now loads the manual's reset values, so its soft reset shows the right ODR, and it clears INT_STATUS on read and ignores writes to the sense registers. `acc_div` and `gyro_div` stayed as configured after a soft reset, though the chip returns to +/-16 g and +/-2000 dps, so 1 g read as 0.125 g after a reset from +/-2 g. `icm42688_soft_reset` now sets the divisors of the reset ranges, which takes the `const` off its device.
- **Still open:** `icm42688_soft_reset` returns at once and the HAL has no delay, so the caller must wait 1 ms before the next register access, or the chip may miss it. The caller must also allow the 1 ms power-up time before `icm42688_init`. With `ICM42688_ODR_6HZ`, `3HZ` and `1HZ` refused, the accel rates below 12.5 Hz cannot be reached: that needs a new call.
- **Suspected:** `icm42688_set_power_mode` cannot honour the 200 us write lockout or the 45 ms gyro minimum on time, and the header does not pass them on.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/icm42688/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_icm42688
```

## Test status

- Unit tests (sim): 54 tests, all passing
- Hardware model tests: 13 tests, all passing
- Null param tests: 9 auto-generated, all passing
- HIL: N/A

## Migration

From the fixes of 2026-11.

| Change | What a caller has to do | If they do nothing |
|--------|-------------------------|--------------------|
| `icm42688_soft_reset` takes `icm42688_t *`, no longer `const icm42688_t *` | Pass a non-const pointer. Nothing changes for a plain `&dev` | A caller holding a `const` pointer no longer compiles |
| After a soft reset, readings are scaled for +/-16 g and +/-2000 dps, as the chip then is | Run `icm42688_init` to get the configured ranges back, as before | Readings are right for the chip's reset ranges, where they were too low by the old range's factor |

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: SPI-only (no I2C support); burst read for temp+accel+gyro in single transaction; power mode enum shared between accel and gyro with per-axis semantics matching datasheet PWR_MGMT0 register layout.

## Responsible human

@L-X-Yao
