# ltr390 — Lite-On LTR-390UV UV and ambient light sensor

**Status: candidate**
**Layer: L1 (drivers)**

I2C driver for the LTR-390UV-01 dual-channel UV and ambient light sensor.
20-bit ALS and UVS channels with configurable gain (1x-18x) and resolution
(13-bit to 20-bit). Fixed I2C address 0x53, part ID verification on init.

## Origin

Proactive build for UV index and ambient light sensing use cases (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- Adafruit_LTR390: Arduino/C++ library, heap-heavy, not C99 compatible.
- No suitable zero-dependency C99 implementation found.

Reference: LTR-390UV-01 datasheet (Lite-On).

## Dependencies

None. I2C HAL injected via function pointers.

## API

```c
/* Error codes: OK=0, ERR_I2C=-1, ERR_ID=-2, ERR_PARAM=-3, ERR_NOT_READY=-4 */

ltr390_err_t ltr390_init(ltr390_t *dev, const ltr390_i2c_t *i2c,
                         const ltr390_cfg_t *cfg);
ltr390_err_t ltr390_read_als(ltr390_t *dev, uint32_t *raw);
ltr390_err_t ltr390_read_uvs(ltr390_t *dev, uint32_t *raw);
ltr390_err_t ltr390_set_mode(ltr390_t *dev, ltr390_mode_t mode);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `CONTRACT_ENABLED` | off | Enable Design by Contract runtime checks |

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against Lite-On LTR-390UV-01, BNS-OD-FC002/A4, version 1.1. Not run on hardware.

- **Matched:** Address, part ID check, register addresses, MAIN_CTRL values, resolution and gain fields, the 20-bit data read.
- **Fixed:** `ltr390_init` wrote the measurement rate bits as 000 (25 ms) while its comment said they stayed at the default, 010 (100 ms). It now writes 010, as the manual's 0x22 example does, so 17, 16 and 13-bit modes sample every 100 ms instead of every 50, 25 and 25 ms. The model has the p.12 reset values, clears status on read, and changes its data only when a conversion ends, after the manual's wake-up and conversion time, in the channel of the mode. It also has software reset. A read straight after `ltr390_init` or `ltr390_set_mode` got zero or old data with `LTR390_OK`, and each read succeeded in the other mode with stale data. All of these now return `LTR390_ERR_NOT_READY`. The new-data bit clears when read, so it is remembered once seen, and reads between conversions go on returning the last result.
- **Still open:** There is no lux or UVI conversion and no call for measurement rate, interrupts, thresholds or software reset. Power-On status is never read, so a reset that restores the defaults goes unnoticed.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ltr390/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ltr390
./build/test_hw_model_ltr390
```

## Test status

- Unit tests (sim): 11 pass (test_ltr390) + 54 pass (test_hw_model_ltr390)
- HIL: N/A

## Migration

From the fixes of 2026-11.

| Change | What a caller has to do | If they do nothing |
|--------|-------------------------|--------------------|
| A read before the first conversion after init or `ltr390_set_mode` returns the new `LTR390_ERR_NOT_READY` and leaves `raw` alone | Wait one measurement period, or retry on this code | A caller that treats any error as fatal stops there. One that ignores the return keeps its old `raw` |
| `ltr390_read_als` in UVS mode and `ltr390_read_uvs` in ALS mode return `LTR390_ERR_NOT_READY`, however long the caller waits | Call `ltr390_set_mode` first, and read each channel in its own mode | The caller gets this error where it got a stale or zero value with `LTR390_OK` |
| `ltr390_init` and `ltr390_set_mode` read MAIN_STATUS once, which also clears its Power-On bit | Nothing, unless you read that bit yourself | The driver has never used the bit |
| `ltr390_t` grew by one `bool` | Nothing: treat it as opaque storage | Nothing changes for the caller |

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: 20-bit raw output (no lux/UVI conversion in driver), mode enum for ALS/UVS switching, config struct for gain/resolution/mode at init.

## Responsible human

@user
