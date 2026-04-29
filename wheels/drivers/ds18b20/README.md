# ds18b20

**STATUS: candidate**
**Layer: L1 (drivers)**

Dallas/Maxim DS18B20 1-Wire digital temperature sensor driver. 12-bit
resolution (0.0625°C), configurable 9-12 bits. Supports single-device
(skip ROM) and multi-device (match ROM) buses.

## Why not VSF native?

Checked VSF source (2026-04-29): no DS18B20 or 1-Wire driver in VSF.

## API

```c
#include "ds18b20.h"

ds18b20_err_t ds18b20_init(ds18b20_t *dev, const ds18b20_ow_t *ow);
ds18b20_err_t ds18b20_read_rom(ds18b20_t *dev);
ds18b20_err_t ds18b20_start_conversion(ds18b20_t *dev);
ds18b20_err_t ds18b20_read_temperature(ds18b20_t *dev, float *temp_c);
ds18b20_err_t ds18b20_set_resolution(ds18b20_t *dev, ds18b20_resolution_t res);
uint8_t       ds18b20_crc8(const uint8_t *data, size_t len);
```

HAL: 1-Wire (reset + write_byte + read_byte).

## How to test

```bash
cd wheels/drivers/ds18b20/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **9 C cases** — init null, 25°C read, negative temp, 85°C power-on
  default, no device, CRC failure, read ROM, CRC8 known vector, start conversion

## Dependencies

1-Wire HAL (injected via callbacks).

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
