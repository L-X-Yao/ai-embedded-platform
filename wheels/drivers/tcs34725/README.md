# tcs34725

**Status: candidate**
**Layer: L1 (drivers)**

AMS TCS34725 I2C RGB color sensor driver. Register-based I2C with command
register protocol (0x80 | reg). Fixed address 0x29, chip ID 0x44 or 0x4D.
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

tcs34725_err_t tcs34725_init(tcs34725_dev_t *dev, const tcs34725_i2c_t *i2c,
                             tcs34725_integ_t integ, tcs34725_gain_t gain);
tcs34725_err_t tcs34725_read_rgbc(tcs34725_dev_t *dev, tcs34725_rgbc_t *out);
tcs34725_err_t tcs34725_set_integration_time(tcs34725_dev_t *dev, tcs34725_integ_t integ);
tcs34725_err_t tcs34725_set_gain(tcs34725_dev_t *dev, tcs34725_gain_t gain);
tcs34725_err_t tcs34725_enable(tcs34725_dev_t *dev);
tcs34725_err_t tcs34725_disable(tcs34725_dev_t *dev);
tcs34725_err_t tcs34725_get_color_temp(const tcs34725_rgbc_t *rgbc, uint16_t *cct_k);
tcs34725_err_t tcs34725_get_lux(const tcs34725_rgbc_t *rgbc, float *lux);
```

HAL: register-based I2C (write_reg + read_regs). Command register protocol
handled internally (driver ORs 0x80 with register address).

## Configuration (compile-time)

No compile-time configuration needed. Integration time and gain are set at
runtime via init or setter functions.

## Supported platforms

- [x] Linux host (sim) — passed 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/tcs34725/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **14 C cases** — init OK, alt chip ID, bad chip ID, null safety,
  I2C failure, bad gain, read RGBC, read null, set integration time, set gain,
  enable/disable, color temp, lux, lux negative clamp

## API convention deviations

None.

## API review

- Date: 2026-05-01
- Key decisions: command register protocol (0x80|reg) handled internally;
  DN40 empirical formulas for CCT and lux

## Responsible human

@user
