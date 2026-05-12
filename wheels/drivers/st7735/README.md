# st7735 — ST7735 128x160 SPI TFT LCD driver (RGB565)

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for ST7735 128x160 pixel SPI TFT LCD displays.
RGB565 16-bit color. SPI interface + DC/RST/CS pins injected via callbacks.
Supports window-based drawing, single pixel, fill rect, and fill screen.

## Origin

Proactive build. ST7735 is one of the most common small TFT displays
in embedded projects (hobby boards, handheld devices, instrument panels).

## Why self-written?

Searched GitHub (2026-05-12). Existing ST7735 libraries (Adafruit, TFT_eSPI)
are tied to Arduino or specific HALs. No suitable zero-dependency, zero-heap
C99 implementation matching our wheel conventions found.

Reference: Sitronix ST7735 datasheet rev 1.4.

## Dependencies

SPI HAL (injected via `st7735_spi_t` callbacks). No other dependencies.

## API

```c
st7735_err_t st7735_init(st7735_t *dev, const st7735_spi_t *spi);
st7735_err_t st7735_set_window(st7735_t *dev, uint16_t x0, uint16_t y0,
                                uint16_t x1, uint16_t y1);
st7735_err_t st7735_write_pixels(st7735_t *dev, const uint16_t *pixels,
                                  uint32_t count);
st7735_err_t st7735_fill_rect(st7735_t *dev, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h, uint16_t color);
st7735_err_t st7735_fill_screen(st7735_t *dev, uint16_t color);
st7735_err_t st7735_draw_pixel(st7735_t *dev, uint16_t x, uint16_t y,
                                uint16_t color);
static inline uint16_t st7735_rgb565(uint8_t r, uint8_t g, uint8_t b);
```

## Configuration (compile-time)

None. Display dimensions fixed at 128x160 via ST7735_WIDTH/ST7735_HEIGHT.

## Supported platforms

- [x] Host (sim) — passed 2026-05-12
- [ ] ARM Cortex-M — not yet verified (uses injected SPI HAL)

## Known issues

- No rotation support (MADCTL fixed at 0x00).
- No display inversion toggle.
- No partial display mode.

## How to test

```bash
cd wheels/drivers/st7735/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_st7735
```

## Test status

- Unit tests (sim): PASS (13 C tests)
- HW model tests: PASS (13 tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: SPI HAL uses transfer() with tx/rx buffer (not single-byte xfer) for potential DMA compatibility; DC/CS/RST pins injected separately for flexibility; RGB565 helper is inline for zero-overhead color conversion; coordinates validated against fixed 128x160 bounds.

## Responsible human

@user
