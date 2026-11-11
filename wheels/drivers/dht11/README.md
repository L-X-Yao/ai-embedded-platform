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

- **Matched:** The 40-bit frame and its byte order, the parity sum with the manual's example, the sensor response and its edge limits, the 40 us bit sample point.
- **Fixed:** The start pulse sat exactly on 18 ms, where the figure shows more than 18 ms, and is now 20 ms. The pin was driven high for 30 us after it, which on a push-pull HAL could fight the sensor, and the host now goes to input at once. The mock discarded time and replayed pin levels in the driver's own read order, so `hw_model_dht11.h` now derives the pin from the manual's timings, and a wrong sample point fails. None of this has run on hardware.
- **Still open:** The header does not say interrupts or preemption must stay off during `dht11_read`: the 40 us sample has about 15 us of margin each side, and only the parity catches a slip.
- **Suspected:** Nothing waits out the first second after power-up, when the manual forbids any command. `dht11_read` returns the measurement from the previous read and does not enforce the interval between reads.

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

None.

## API review

- Proposal date: 2026-04-29
- Reviewer: @L-X-Yao (batch-generated, pending review)

## Responsible human

@L-X-Yao
