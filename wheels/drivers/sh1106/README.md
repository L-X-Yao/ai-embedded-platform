# sh1106 — SH1106 132x64 OLED display controller driver

**Status: candidate**
**Layer: L1 (drivers)**

SH1106-based 1.3" OLED display driver over I2C. Framebuffer-based: caller provides a
1024-byte buffer, draws pixels into it, then flushes to the display. Key difference from
SSD1306: uses page addressing with a 2-column offset (columns 2-129 for 128px width).

## Origin

Proactive build for common OLED display use cases (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Existing libraries (u8g2, Adafruit SH1106) are C++ or framework-dependent.
- No suitable zero-dependency C99 implementation found.

Reference: SH1106 datasheet (Sino Wealth).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
sh1106_err_t sh1106_init(sh1106_t *dev, const sh1106_i2c_t *i2c,
                         uint8_t addr, uint8_t *framebuf);
sh1106_err_t sh1106_display(sh1106_t *dev);
void         sh1106_clear(sh1106_t *dev);
void         sh1106_set_pixel(sh1106_t *dev, uint8_t x, uint8_t y, bool on);
bool         sh1106_get_pixel(const sh1106_t *dev, uint8_t x, uint8_t y);
void         sh1106_fill(sh1106_t *dev, bool on);
sh1106_err_t sh1106_set_contrast(sh1106_t *dev, uint8_t val);
sh1106_err_t sh1106_display_on(const sh1106_t *dev);
sh1106_err_t sh1106_display_off(const sh1106_t *dev);
```

## Supported platforms

- [x] Host (sim) — 2026-05-11
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/sh1106/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sh1106 && ./build/test_hw_model_sh1106 && ./build/test_null_sh1106
```

## Test status

- Unit tests (sim): 35 passed (20 unit + 4 hw_model + 11 null)
- HIL: N/A

## API convention deviations

None.

## API Review

Pending.

## Responsible human

@user
