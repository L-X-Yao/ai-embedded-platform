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
bh1750_err_t bh1750_read_lux(const bh1750_t *dev, float *lux);
bh1750_err_t bh1750_power_on(const bh1750_t *dev);
bh1750_err_t bh1750_power_off(const bh1750_t *dev);
bh1750_err_t bh1750_reset(const bh1750_t *dev);
```

HAL: command-based I2C (write raw bytes, read raw bytes — no register address).

A measurement takes up to 180 ms, or 24 ms in the L modes, and the driver waits
it out through `delay_ms`:

| `delay_ms` | Continuous modes | One-time modes |
|---|---|---|
| set | init and `bh1750_power_on` block for one measurement; reads do not block | init blocks for one measurement, and each read measures, so it blocks too |
| NULL | the caller waits before the first read, or it gets 0 with `BH1750_OK` | init returns `BH1750_ERR_PARAM` |

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Datasheet check

Checked on 2026-11-06 against ROHM BH1750FVI Technical Note, 2011.11 - Rev.D. Not run on hardware.

- **Matched:** Addresses, opcodes, one opcode per write, the read format, the lux formula with the manual's worked examples, the init order.
- **Fixed:** Nothing waited for a measurement, so a read straight after init gave 0 lx with `BH1750_OK`. init now waits the maximum time, 180 ms or 24 ms in L mode. One-time modes measured only in init, so every read returned the first result: each read now measures, which needs `delay_ms`, and init refuses a one-time mode without it. `bh1750_power_on` left a continuous mode stopped, so reads kept the value from before the power off, and it now restarts it. The model returned a fixed value in any state and handled one-time modes like continuous ones. It also cleared the data on Reset in Power Down, where the chip ignores it, and answered at any address. None of this has run on hardware.
- **Still open:** With `delay_ms` NULL in a continuous mode, a read that comes too early still gives 0 or an old value with `BH1750_OK`, because the driver has no clock. MTreg is not supported, so there is no optical window adjustment and no 0.11 lx range. The 1.2 factor is typical, 0.96 to 1.44 by part, with no calibration input. The mode cannot change after init. The DVI reset is left to the board.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/bh1750/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **12 C cases** — init null, init cmd verification, H-res lux,
  H-res2 lux (0.5x), zero lux, I2C failure, power off/on, read null, one-time
  reads; **24 hw_model cases** with the manual's measurement times

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@L-X-Yao
