# gc9a01 — GC9A01 240x240 round TFT LCD SPI driver

**Status: candidate**
**Layer: L1 (drivers)**

RGB565 TFT LCD driver for the GC9A01 round display controller (common on 1.28" smartwatch
and gauge displays). Supports hardware reset, rotation (0/90/180/270), sleep mode,
display on/off, window addressing, pixel streaming, full-screen fill, and RGB565 color
conversion. Pure C99, no heap, no OS.

## Origin

Proactive build for smartwatch/gauge display-equipped embedded projects.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-07):

- Adafruit/Arduino GC9A01 libs: C++ / Arduino-coupled, not portable C99.
- TFT_eSPI: ESP-specific, heavy C++ dependency.
- LVGL drivers: tied to LVGL framework, not standalone.
- No suitable zero-dependency C99 implementation found that fits our wheel contract (no OS, DI via function pointers, multi-instance).

Reference: GalaxyCore GC9A01 datasheet.

## Dependencies

None. SPI HAL injected via function pointers.

## API

```c
gc9a01_err_t gc9a01_init(gc9a01_t *dev, const gc9a01_spi_t *spi);

gc9a01_err_t gc9a01_set_window(gc9a01_t *dev,
                               uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1);

gc9a01_err_t gc9a01_write_pixels(gc9a01_t *dev,
                                 const uint16_t *pixels, uint32_t count);

gc9a01_err_t gc9a01_fill(gc9a01_t *dev, uint16_t color);

gc9a01_err_t gc9a01_set_rotation(gc9a01_t *dev, uint8_t rotation);

gc9a01_err_t gc9a01_sleep(gc9a01_t *dev, bool enable);

gc9a01_err_t gc9a01_display(gc9a01_t *dev, bool on);

uint16_t gc9a01_rgb565(uint8_t r, uint8_t g, uint8_t b);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| (none) | N/A     | All configuration is runtime via API calls |

Display is fixed at 240x240 (GC9A01 is a round LCD controller).

## Supported platforms

- [x] Host (sim) -- 2026-05-07
- [ ] Real hardware target -- not yet verified

## Datasheet check

Checked on 2026-11-06 against LCD-DST-3014 GC9A01 Datasheet V1.0 Preliminary (GC9A01A, Rev.1.0 Preliminary, 2019-07-02). Not run on hardware.

- **Matched:** Command codes, MADCTL bits MY to BGR, COLMOD 0x05, CASET and RASET format, the 240x240 area, RGB565 byte order, D/CX and CSX use, the wait after SLPOUT.
- **Fixed:** `gc9a01_sleep(dev, true)` returned without the 120 ms the manual requires before SLPOUT. `GC9A01_MADCTL_MH` was 0x02; the bit is 0x04. The model resets COLMOD to 0x66 and has separate power-on, hardware and software resets, with MADCTL and COLMOD kept on a software reset.
- **Still open:** There is no chip ID check, so a missing panel goes unnoticed. No API for INVOFF, TE, partial, idle, scroll, brightness or gamma. Only the 4-line serial interface is supported. Init sends no vendor init table, which Zephyr calls "essential for proper operation of the display controller", so a real panel may show no usable picture. It is not added because the tables differ by panel (30 of 51 registers across Adafruit, Espressif and TFT_eSPI) and none can be tried here. A caller-supplied table, as Espressif has, would change the API.
- **Suspected:** BGR is always 0, so a BGR panel swaps red and blue, and rotation 0 may be mirrored there. Init sends 0x01 as a software reset, which the manual's command list never defines.

## Known issues

- `fill` and `write_pixels` send pixels one-by-one via `xfer()`. For performance on real hardware, consider a bulk SPI DMA transfer in the HAL layer.
- GC9A01 INVON is sent during init (required for correct colors on most round LCD panels).

## How to test

```bash
cd wheels/drivers/gc9a01/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_gc9a01
./build/test_hw_model_gc9a01
./build/test_null_gc9a01
```

## Test status

- Unit tests (sim): 59 pass, 0 fail
- HW model tests (sim): 18 pass, 0 fail
- Null tests (auto-generated): 9 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## SPI HAL

```c
typedef struct {
    void    (*cs_low)(void *ctx);
    void    (*cs_high)(void *ctx);
    uint8_t (*xfer)(void *ctx, uint8_t tx);
    void    (*dc_cmd)(void *ctx);    /* DC pin low  (command) */
    void    (*dc_data)(void *ctx);   /* DC pin high (data)    */
    void    (*rst_low)(void *ctx);   /* RST pin low  (reset)  */
    void    (*rst_high)(void *ctx);  /* RST pin high (normal) */
    void    (*delay_ms)(void *ctx, uint16_t ms);
    void    *ctx;
} gc9a01_spi_t;
```

## API review

- **Date**: 2026-05-07
- **Key decisions**: RGB565 only (most common for embedded TFT), rotation via MADCTL, dc_cmd/dc_data separate callbacks (matches 4-wire SPI wiring), fixed 240x240 (no configurable size since GC9A01 is always 240x240 round)

## Responsible human

@L-X-Yao
