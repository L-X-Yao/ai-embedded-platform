# max7219

**Status: candidate**
**Layer: L1 (drivers)**

Maxim MAX7219 LED matrix / 7-segment display driver. SPI interface with
16-bit shift register (register address + data). Supports 8 digits/rows,
16-level intensity, BCD and raw decode modes.
## API

```c
#include "max7219.h"

max7219_err_t max7219_init(max7219_t *dev, const max7219_spi_t *spi);
max7219_err_t max7219_set_intensity(max7219_t *dev, uint8_t level);
max7219_err_t max7219_set_scan_limit(max7219_t *dev, uint8_t digits);
max7219_err_t max7219_shutdown(max7219_t *dev, bool shutdown);
max7219_err_t max7219_set_decode_mode(max7219_t *dev, uint8_t mask);
max7219_err_t max7219_set_digit(max7219_t *dev, uint8_t digit, uint8_t value);
max7219_err_t max7219_clear(max7219_t *dev);
max7219_err_t max7219_display_test(max7219_t *dev, bool enable);
```

HAL: SPI (transfer + CS control).

## How to test

```bash
cd wheels/drivers/max7219/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **10 C cases** — init null, init sequence verification, set digit,
  digit OOB, intensity, intensity clamp, shutdown, clear, SPI failure, scan limit

## Dependencies

SPI HAL (injected via callbacks).

## Origin

Proactive build for common embedded components (2026-04-29 batch).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

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
