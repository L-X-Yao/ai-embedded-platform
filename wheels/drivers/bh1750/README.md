# bh1750

**Status: candidate**
**Layer: L1 (drivers)**

ROHM BH1750FVI ambient light sensor driver. **Command-based I2C** (not
register-based): write command byte, read 2 data bytes. Supports continuous
and one-time measurement modes at high/low resolution (1 lx / 0.5 lx / 4 lx).

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Found ebrezadev/BH1750FVI (MIT). Simple sensor, all impls similar. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

I2C HAL (injected via callbacks, command-based pattern).

## API

```c
#include "bh1750.h"

bh1750_err_t bh1750_init(bh1750_t *dev, const bh1750_i2c_t *i2c,
                         const bh1750_cfg_t *cfg);
bh1750_err_t bh1750_read_lux(bh1750_t *dev, float *lux);
bh1750_err_t bh1750_power_on(bh1750_t *dev);
bh1750_err_t bh1750_power_off(bh1750_t *dev);
bh1750_err_t bh1750_reset(bh1750_t *dev);
```

HAL: command-based I2C (write raw bytes, read raw bytes — no register address).

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Known issues

None.

## How to test

```bash
cd wheels/drivers/bh1750/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **8 C cases** — init null, init cmd verification, H-res lux,
  H-res2 lux (0.5x), zero lux, I2C failure, power off/on, read null

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
