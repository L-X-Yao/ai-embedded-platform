# framebuffer -- pixel buffer with drawing primitives for mono/RGB565 displays

**Status: candidate**
**Layer: L0 (primitives)**

Pixel buffer that manipulates a caller-provided byte array. Supports monochrome
(VLSB for SSD1306, HLSB) and RGB565 formats. Includes set/get pixel, lines
(horizontal, vertical, Bresenham), rectangles, circles (midpoint), and 1-bit
bitmap blitting. All drawing clips to bounds. No heap, no OS.

## Origin

Proactive build for display rendering across OLED (SSD1306, SH1106) and
color TFT (ST7789, ILI9341, GC9A01) driver wheels. Decouples pixel-level
drawing from hardware transport.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- Adafruit GFX: C++ only, heap-heavy, tied to Arduino
- u8g2: Monochrome only, large footprint, complex integration
- LVGL: Full GUI toolkit, massive footprint, overkill for primitive drawing
- No suitable zero-dependency C99 implementation with multi-format support found.

Reference: SSD1306 datasheet (vertical LSB layout), MicroPython framebuf module.

## Dependencies

None.

## API

```c
typedef enum {
    FB_FORMAT_MONO_VLSB = 0,  /* 1bpp, vertical LSB first (SSD1306) */
    FB_FORMAT_MONO_HLSB = 1,  /* 1bpp, horizontal LSB first */
    FB_FORMAT_RGB565    = 2,  /* 16bpp, RGB565 big-endian */
} fb_format_t;

uint32_t framebuffer_buf_size(uint16_t width, uint16_t height, fb_format_t format);
framebuffer_err_t framebuffer_init(framebuffer_t *fb, uint8_t *buf,
                                   uint16_t width, uint16_t height, fb_format_t format);
void framebuffer_clear(framebuffer_t *fb, uint16_t color);
void framebuffer_set_pixel(framebuffer_t *fb, uint16_t x, uint16_t y, uint16_t color);
uint16_t framebuffer_get_pixel(const framebuffer_t *fb, uint16_t x, uint16_t y);
void framebuffer_hline(framebuffer_t *fb, uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void framebuffer_vline(framebuffer_t *fb, uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void framebuffer_rect(framebuffer_t *fb, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint16_t color);
void framebuffer_fill_rect(framebuffer_t *fb, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h, uint16_t color);
void framebuffer_line(framebuffer_t *fb, int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1, uint16_t color);
void framebuffer_circle(framebuffer_t *fb, int16_t cx, int16_t cy,
                        uint16_t r, uint16_t color);
void framebuffer_blit_1bpp(framebuffer_t *fb, uint16_t x, uint16_t y,
                           const uint8_t *bitmap, uint16_t bw, uint16_t bh, uint16_t color);
```

## Configuration (compile-time)

No compile-time configuration. Format is selected at runtime via `fb_format_t`.

## Supported platforms

- [x] Host (sim) -- 2026-05-12
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/primitives/framebuffer/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_framebuffer
```

## Test status

- Unit tests (sim): 16 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: Signed coordinates for line/circle (Bresenham needs negative intermediates). blit_1bpp uses MSB-first row-major format (standard font glyph layout). RGB565 stored big-endian (matches most display controllers).

## Responsible human

@user
