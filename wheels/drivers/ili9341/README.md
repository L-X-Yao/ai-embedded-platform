# ili9341 -- ILI9341 SPI TFT display driver (240x320, RGB565)

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the ILI9341 240x320 TFT LCD controller.
SPI interface with D/C and RST pins injected via callbacks. RGB565
color mode with support for 0/90/180/270 degree rotation, rectangle
fill, single pixel draw, and full screen clear.

## Origin

Proactive build. ILI9341 is the most common SPI TFT controller in
hobby and industrial embedded displays (2.4" and 2.8" TFT modules).

## Why not VSF native?

VSF has a GUI subsystem but no standalone ILI9341 SPI driver. Self-written
for lightweight projects that need direct display control without the
full VSF GUI stack.

## Dependencies

SPI HAL with D/C and RST pins (injected via `ili9341_spi_t` callbacks).
No other dependencies.

## API

```c
ili9341_err_t ili9341_init(ili9341_t *dev, const ili9341_spi_t *spi);
ili9341_err_t ili9341_set_rotation(ili9341_t *dev, ili9341_rotation_t rot);
ili9341_err_t ili9341_set_window(ili9341_t *dev, uint16_t x0, uint16_t y0,
                                uint16_t x1, uint16_t y1);
ili9341_err_t ili9341_fill_rect(ili9341_t *dev, uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h, uint16_t color);
ili9341_err_t ili9341_draw_pixel(ili9341_t *dev, uint16_t x, uint16_t y,
                                 uint16_t color);
ili9341_err_t ili9341_fill_screen(ili9341_t *dev, uint16_t color);
uint16_t      ili9341_rgb565(uint8_t r, uint8_t g, uint8_t b);
```

## Usage

```c
ili9341_t lcd;
ili9341_init(&lcd, &spi_hal);
ili9341_set_rotation(&lcd, ILI9341_ROT_90);
ili9341_fill_screen(&lcd, ili9341_rgb565(0, 0, 0));
ili9341_fill_rect(&lcd, 10, 10, 50, 50, ili9341_rgb565(255, 0, 0));
```

## Configuration (compile-time)

None. Rotation is runtime-configurable.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected SPI HAL)

## Known issues

- No framebuffer or DMA support; single-byte SPI transfer only.
- No text rendering (pair with `font_5x7` wheel for basic text).

## How to test

```bash
cd wheels/drivers/ili9341/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ili9341
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## Responsible human

@user
