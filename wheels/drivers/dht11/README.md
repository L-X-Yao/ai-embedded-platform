# dht11

**STATUS: candidate**
**Layer: L1 (drivers)**

DHT11 temperature and humidity sensor driver. Single-wire protocol:
host pulls low 18ms, sensor responds with 40 bits (2 bytes humidity +
2 bytes temperature + 1 byte checksum).

## Why not VSF native?

Checked VSF source (2026-04-29): no DHT11/DHT22 driver in service/utilities/component.

## API

```c
#include "dht11.h"

dht11_err_t dht11_init(dht11_t *dev, const dht11_gpio_t *gpio);
dht11_err_t dht11_read(dht11_t *dev, dht11_data_t *out);
```

HAL: GPIO direction control + pin read/write + microsecond delay.

## How to test

```bash
cd wheels/drivers/dht11/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **7 C cases** — init null/missing callback, normal read, high humidity,
  zero values, checksum failure, read null

## Dependencies

GPIO HAL (injected via callbacks).

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
