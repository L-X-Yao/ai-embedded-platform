# tcs34725

**Status: candidate**
**Layer: L1 (drivers)**

AMS TCS34725 I2C RGB color sensor driver. Register-based I2C with command
register protocol (0x80 | reg, and 0xA0 | reg for the RGBC read). Fixed
address 0x29, chip ID 0x44 or 0x4D.
Provides RGBC raw data, estimated color temperature (CCT), and lux output.

## Origin

Proactive build for common embedded components (2026-05-01).

## Why self-written?

Searched GitHub (2026-05-02). Found libdriver/tcs34725 (MIT). Standard I2C register access. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

I2C HAL (injected via callbacks, register-based pattern).

## API

```c
#include "tcs34725.h"

tcs34725_err_t tcs34725_init(tcs34725_t *dev, const tcs34725_i2c_t *i2c,
                             tcs34725_integ_t integ, tcs34725_gain_t gain);
tcs34725_err_t tcs34725_read_rgbc(tcs34725_t *dev, tcs34725_rgbc_t *out);
tcs34725_err_t tcs34725_set_integration_time(tcs34725_t *dev, tcs34725_integ_t integ);
tcs34725_err_t tcs34725_set_gain(tcs34725_t *dev, tcs34725_gain_t gain);
tcs34725_err_t tcs34725_enable(tcs34725_t *dev);
tcs34725_err_t tcs34725_disable(tcs34725_t *dev);
tcs34725_err_t tcs34725_get_color_temp(const tcs34725_rgbc_t *rgbc, uint16_t *cct_k);
tcs34725_err_t tcs34725_get_lux(const tcs34725_rgbc_t *rgbc, float *lux);
```

HAL: register-based I2C (write_reg + read_regs) and a required delay_ms.
Command register protocol handled internally (driver ORs 0x80 with register
address, or 0xA0 for the RGBC read).

## Configuration (compile-time)

No compile-time configuration needed. Integration time and gain are set at
runtime via init or setter functions.

## Supported platforms

- [x] Linux host (sim) — passed 2026-05-01
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against TAOS135 - AUGUST 2012. Not run on hardware.

- **Matched:** Address, ID values, CMD bit, register addresses, PON and AEN bits, gain codes, the ATIME values from Table 6, channel and byte order.
- **Fixed:** The 8-byte RGBC read used TYPE 00b, which by the manual repeats CDATAL and gives the same value on all four channels, so it now sends 0xA0 | reg. A real part may be more forgiving; that is untested. `tcs34725_init` and `tcs34725_enable` set PON and AEN in one write, against the 2.4 ms the manual asks for. They now wait 3 ms between them, so `delay_ms` is required. The comment on `TCS34725_INTEG_50MS` said 20 cycles; 0xEB is 21. The model now honours TYPE and starts an RGBC only when AEN rises 2.4 ms after PON. `tcs34725_read_rgbc` returned zeros with `TCS34725_OK` until the first cycle ended, and now returns `TCS34725_ERR_NOT_READY`. The model now resets ATIME and WTIME to 0xFF, has STATUS, and fills the data and AVALID only at the end of a cycle.
- **Still open:** After a change of gain or integration time `tcs34725_read_rgbc` returns counts from the old setting. No check for saturation at (256 - ATIME) x 1024. No wait state, thresholds, persistence or interrupt support.
- **Suspected:** `TCS34725_INTEG_101MS` is 0xD5 as Table 6 prints, but the formula makes that 43 cycles, 103 ms. `TCS34725_INTEG_700MS` follows Table 6, but the formula gives 614 ms. `tcs34725_get_color_temp` puts one value in both halves of its formula, so the result may be far off; DN40 is not in the manual set. `tcs34725_get_lux` takes neither gain nor integration time, so its result scales with both, and fixing that changes the API.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/tcs34725/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **17 C cases** — init OK, alt chip ID, bad chip ID, null safety,
  I2C failure, missing delay, PON and AEN write failures, bad gain, read RGBC,
  read null, set integration time, set gain, enable/disable, color temp, lux,
  lux negative clamp
- Model tests: **18 C cases**

## API convention deviations

None.

## API review

- Date: 2026-05-01
- Key decisions: command register protocol (0x80|reg) handled internally;
  DN40 empirical formulas for CCT and lux

## Responsible human

@user
