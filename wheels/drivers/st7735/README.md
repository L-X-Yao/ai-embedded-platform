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
uint16_t st7735_rgb565(uint8_t r, uint8_t g, uint8_t b);  /* static inline in header */
```

## Configuration (compile-time)

None. Display dimensions fixed at 128x160 via ST7735_WIDTH/ST7735_HEIGHT.

## Supported platforms

- [x] Host (sim) — passed 2026-05-12
- [ ] ARM Cortex-M — not yet verified (uses injected SPI HAL)

## Datasheet check

Checked on 2026-11-06 against Sitronix ST7735, V2.1, 2010-02-01. The rev 1.4 that this README and the model cite was not available. Not run on hardware.

- **Matched:** All eight command codes, the 128x160 limits, CASET and RASET format, the reset and SLPOUT waits, D/CX and CSX use, RGB565 packing, MADCTL 0x00 as an RGB panel.
- **Still open:** The model starts with normal mode off where the manual says Normal, clears MADCTL and COLMOD on a software reset, which the manual leaves unchanged, takes bytes with CSX high, and has no time base, so it cannot see a command sent inside a 120 ms wait. There is no GM=000 support, so a module wired for 132x162 memory cannot get its window offsets. MADCTL is fixed, so a BGR panel swaps red and blue. Every `st7735_write_pixels` sends RAMWR and returns to the window start, so a window must be filled in one call. Init turns the display on with frame memory still random, so garbage shows until the first fill. The HAL contract is unwritten: RESX low at least 10 us, SCL write cycle at least 66 ns, SPI4W strapped high. ST7735R and ST7735S parts have their own manuals, which were not checked.
- **Suspected:** COLMOD is sent as 0x05 where one note asks for 55h, though the same page and p52 allow 0x05.

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
