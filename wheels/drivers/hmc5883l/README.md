# hmc5883l

**Status: candidate**
**Layer: L1 (drivers)**

Honeywell HMC5883L 3-axis digital magnetometer driver. I2C register-based
interface, fixed address 0x1E. Configurable gain (0.88-8.1 Ga range) and
data rate (0.75-75 Hz). Outputs raw counts or Gauss.

Note: data register order is X, Z, Y (not X, Y, Z).

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. Discontinued chip; no well-maintained C driver. Self-written with HAL injection per platform conventions.

## Dependencies

I2C HAL (injected via callbacks, register-based pattern).

## API

```c
#include "hmc5883l.h"

hmc5883l_err_t hmc5883l_init(hmc5883l_t *dev, const hmc5883l_i2c_t *i2c,
                             const hmc5883l_cfg_t *cfg);
hmc5883l_err_t hmc5883l_read_raw(const hmc5883l_t *dev, hmc5883l_raw_t *raw);
hmc5883l_err_t hmc5883l_read_gauss(const hmc5883l_t *dev, hmc5883l_data_t *out);
```

HAL: I2C register-based (write_reg/read_regs).

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Datasheet check

Checked on 2026-11-06 against Honeywell Form 900405 Rev E. Not run on hardware.

- **Matched:** Address, ID registers, register map, data order X, Z, Y, rate and gain codes, the Gain column scaling.
- **Fixed:** The -4096 overflow marker was returned as a field value, and is now `HMC5883L_ERR_OVERFLOW`. With `delay_ms` set, `hmc5883l_init` now waits for the first set, drops it because it still has the old gain, and waits for the next, which takes about 3 output periods: 200 ms at 15 Hz, 4 s at 0.75 Hz. The file comment said milligauss where the code gives Gauss. That wait no longer depends on a read clearing RDY, which the manual gives only for single-measurement mode. The model now has the reset values, takes 2/fDO to the first set, sets RDY and LOCK, and applies a new gain from the second measurement.
- **Still open:** With `delay_ms` NULL, reads right after init still return old data.
- **Suspected:** init does not wait the 200 us for I2C or 50 ms for the analog circuit after power-up.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/hmc5883l/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **8 C cases** — init null, ID check, bad ID, raw read, Gauss
  conversion, different gain, I2C failure, read null

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
