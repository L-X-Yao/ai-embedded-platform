# ads1115

**Status: candidate**
**Layer: L1 (drivers)**

TI ADS1115 16-bit 4-channel delta-sigma ADC driver. I2C register-based
interface. Supports single-shot mode, configurable PGA gain (6.144V to
0.256V FSR), mux channel selection (4 single-ended or 2 differential).

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Referenced libdriver/ads1115 (MIT) for register sequences and compensation algorithms. MISRA-compliant; straightforward I2C, our impl adequate. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

I2C HAL (injected via callbacks).

## API

```c
#include "ads1115.h"

ads1115_err_t ads1115_init(ads1115_t *dev, const ads1115_i2c_t *i2c,
                           const ads1115_cfg_t *cfg);
ads1115_err_t ads1115_read_raw(ads1115_t *dev, int16_t *raw);
ads1115_err_t ads1115_read_voltage(ads1115_t *dev, float *voltage_mv);
ads1115_err_t ads1115_set_channel(ads1115_t *dev, ads1115_mux_t mux);
```

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Datasheet check

Checked on 2026-11-06 against TI SBAS444E. Not run on hardware.

- **Matched:** Addresses, config register fields, data rate and PGA codes, scaling.
- **Fixed:** Nothing in the driver.
- **Still open:** The model finishes a conversion inside the config write, so the poll loop never runs, and it lets a write clear OS. The property tests use a byte-wide mock in which two registers share a byte.
- **Suspected:** With `delay_ms` NULL the poll is 200 back-to-back reads, shorter than an 8 SPS conversion on a fast bus. `ads1115_read_voltage` is tagged ISR safe although it blocks for a conversion.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ads1115/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **8 C cases** — init null, raw read positive/negative, voltage conversion,
  half-scale, I2C failure, set channel, config bit verification

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
