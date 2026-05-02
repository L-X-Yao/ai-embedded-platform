# hmc5883l

**STATUS: candidate**
**Layer: L1 (drivers)**

Honeywell HMC5883L 3-axis digital magnetometer driver. I2C register-based
interface, fixed address 0x1E. Configurable gain (0.88-8.1 Ga range) and
data rate (0.75-75 Hz). Outputs raw counts or Gauss.

Note: data register order is X, Z, Y (not X, Y, Z).
## API

```c
#include "hmc5883l.h"

hmc5883l_err_t hmc5883l_init(hmc5883l_t *dev, const hmc5883l_i2c_t *i2c,
                             const hmc5883l_cfg_t *cfg);
hmc5883l_err_t hmc5883l_read_raw(hmc5883l_t *dev, hmc5883l_raw_t *raw);
hmc5883l_err_t hmc5883l_read_gauss(hmc5883l_t *dev, hmc5883l_data_t *out);
```

HAL: I2C register-based (write_reg/read_regs).

## How to test

```bash
cd wheels/drivers/hmc5883l/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **8 C cases** — init null, ID check, bad ID, raw read, Gauss
  conversion, different gain, I2C failure, read null

## Dependencies

I2C HAL (injected via callbacks, register-based pattern).

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Known issues

None.

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## API convention deviations

无。

## Responsible human

@user
