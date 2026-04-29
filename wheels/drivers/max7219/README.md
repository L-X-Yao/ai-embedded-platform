# max7219

**STATUS: candidate**
**Layer: L1 (drivers)**

Maxim MAX7219 LED matrix / 7-segment display driver. SPI interface with
16-bit shift register (register address + data). Supports 8 digits/rows,
16-level intensity, BCD and raw decode modes.

## Why not VSF native?

Checked VSF source (2026-04-29): no MAX7219 driver in VSF.

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
