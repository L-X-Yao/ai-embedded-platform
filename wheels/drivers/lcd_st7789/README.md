# lcd_st7789 — ST7789 240x320 TFT LCD SPI driver

**Status: candidate**
**Layer: L1 (drivers)**

RGB565 TFT LCD driver for the ST7789 controller (common on 1.3"/1.54"/2.0" IPS displays).
Supports hardware reset, rotation (0/90/180/270), color inversion, window addressing,
pixel streaming, and rectangle fill. Pure C99, no heap, no OS.

## Origin

Proactive build for display-equipped embedded projects (sensor dashboards, UI panels).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- Adafruit/Arduino ST7789 libs: C++ / Arduino-coupled, not portable C99.
- TFT_eSPI: ESP-specific, heavy C++ dependency.
- LVGL drivers: tied to LVGL framework, not standalone.
- No suitable zero-dependency C99 implementation found that fits our wheel contract (no OS, DI via function pointers, multi-instance).

Reference: ST7789V datasheet (Sitronix).

## Dependencies

None. SPI HAL injected via function pointers.

## API

```c
lcd_st7789_err_t lcd_st7789_init(lcd_st7789_t *dev,
                                 const lcd_st7789_spi_t *spi,
                                 const lcd_st7789_cfg_t *cfg);

lcd_st7789_err_t lcd_st7789_set_window(lcd_st7789_t *dev,
                                       uint16_t x0, uint16_t y0,
                                       uint16_t x1, uint16_t y1);

lcd_st7789_err_t lcd_st7789_write_pixels(lcd_st7789_t *dev,
                                         const uint8_t *data, size_t len);

lcd_st7789_err_t lcd_st7789_fill_rect(lcd_st7789_t *dev,
                                      uint16_t x, uint16_t y,
                                      uint16_t w, uint16_t h,
                                      uint16_t color);

lcd_st7789_err_t lcd_st7789_display_on(lcd_st7789_t *dev);
lcd_st7789_err_t lcd_st7789_display_off(lcd_st7789_t *dev);

lcd_st7789_err_t lcd_st7789_set_rotation(lcd_st7789_t *dev,
                                         lcd_st7789_rotation_t rot);

lcd_st7789_err_t lcd_st7789_invert(lcd_st7789_t *dev, bool on);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| (none) | N/A     | All configuration is runtime via `lcd_st7789_cfg_t` |

Runtime config via `lcd_st7789_cfg_t`:
- `width` / `height`: native panel dimensions (typ. 240x320)
- `col_offset` / `row_offset`: pixel offset for panels smaller than controller memory

## Supported platforms

- [x] Host (sim) — 2026-05-06
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against Sitronix ST7789VW V1.0. Not run on hardware.

- **Matched:** Command codes, MADCTL bits, rotation values, COLMOD, CASET and RASET layout, RGB565 byte order, reset and sleep waits.
- **Fixed:** init accepted a panel and offsets larger than the frame memory. The model reset MADCTL and COLMOD on SWRESET, which keeps them.
- **Suspected:** Offsets may be wrong at 90, 180 and 270 degrees on panels not centred in the frame memory, such as 240x240; the proof is Figure 33, which is an image. `lcd_st7789_write_pixels` sends RAMWR on each call, so a window written in several calls restarts at its beginning. DISPON goes out with random frame memory, so the panel shows noise until the first draw.

## Known issues

- `fill_rect` sends pixels one-by-one via `xfer()`. For performance on real hardware, consider a bulk SPI DMA transfer in the HAL layer.
- ST7789 INVON is sent during init (required for correct colors on most IPS panels).

## How to test

```bash
cd wheels/drivers/lcd_st7789/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lcd_st7789
```

## Test status

- Unit tests (sim): 51 pass, 0 fail
- Null tests (auto-generated): 11 pass
- HIL: N/A

## API convention deviations

None.

## SPI HAL

```c
typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*dc_set)(void *ctx, bool level);   /* D/C pin: 0=cmd, 1=data */
    void    (*rst_set)(void *ctx, bool level);  /* RST pin: 0=reset, 1=normal */
    void    (*delay_ms)(void *ctx, uint32_t ms);
    void    *ctx;
} lcd_st7789_spi_t;
```

## API review

- **Date**: 2026-05-06
- **Key decisions**: RGB565 only (most common for embedded TFT), rotation via MADCTL, offsets for sub-240x320 panels

## Responsible human

@L-X-Yao
