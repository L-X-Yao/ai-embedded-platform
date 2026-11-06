# dht11

**Status: candidate**
**Layer: L1 (drivers)**

DHT11 temperature and humidity sensor driver. Single-wire protocol:
host pulls low 18ms, sensor responds with 40 bits (2 bytes humidity +
2 bytes temperature + 1 byte checksum).

## Origin

Proactive build for common embedded components (2026-04-29 batch).

## Why self-written?

Searched GitHub (2026-05-02). Found libdriver/dht11 (MIT). Simple single-wire protocol. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

GPIO HAL (injected via callbacks).

## API

```c
#include "dht11.h"

dht11_err_t dht11_init(dht11_t *dev, const dht11_gpio_t *gpio);
dht11_err_t dht11_read(dht11_t *dev, dht11_data_t *out);
```

HAL: GPIO direction control + pin read/write + microsecond delay.

## Supported platforms

- [x] Linux host (sim) — passed 2026-04-29
- [x] Windows (TDM-GCC) — passed 2026-04-29

## Datasheet check

Checked on 2026-11-06 against the DHT11 Product Manual from Aosong Guangzhou Electronics, which prints no document number or revision. Not run on hardware.

- **Matched:** The 40-bit frame and its byte order, the parity sum with the manual's example, the 18 ms host start, the sensor response and its edge limits, the 40 us bit sample point.
- **Still open:** The mock discards time and replays pin levels in the driver's own read order, and there is no hw_model, so the tests cannot catch a wrong sample point or start pulse. The header does not say interrupts or preemption must stay off during `dht11_read`: the 40 us sample has about 15 us of margin each side, and only the parity catches a slip.
- **Suspected:** Nothing waits out the first second after power-up, when the manual forbids any command. `dht11_read` returns the measurement from the previous read and does not enforce the interval between reads. The pin is driven high for 30 us after the start pulse, which on a push-pull HAL could fight the sensor's response. The start pulse sits exactly on the 18 ms minimum, so a `delay_us` that runs short misses it.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/dht11/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **7 C cases** — init null/missing callback, normal read, high humidity,
  zero values, checksum failure, read null

## API convention deviations

无。

## API review

- 提案日期：2026-04-29
- 审批人：@user（批量生成，待审）

## Responsible human

@user
