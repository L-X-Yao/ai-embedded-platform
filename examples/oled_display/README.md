# OLED Display

Show BMP280 temperature and pressure readings on a 128x64 SSD1306 OLED,
using printf_tiny for heap-free formatting and font_5x7 for text rendering.

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `printf_tiny` | Minimal printf -- formats integers without heap |
| L0 | `font_5x7` | 5x7 bitmap font for display text rendering |
| L1 | `oled_ssd1306` | SSD1306 128x64 OLED framebuffer driver |
| L1 | `bmp280` | I2C temperature + pressure sensor |

## What It Demonstrates

1. **I2C bus sharing** -- BMP280 (0x76) and SSD1306 (0x3C) share the
   same I2C bus. Each gets separate HAL callbacks but they route to
   the same physical peripheral.

2. **Framebuffer pattern** -- The OLED driver renders into a
   caller-provided 1024-byte buffer. The application draws text using
   font_5x7, then flushes the entire buffer to the display.

3. **printf_tiny for embedded** -- No `sprintf` or heap allocation.
   Integer formatting via `snprintf_tiny` with fixed-point temperature
   display (multiply by 10, print integer + decimal separately).

4. **Text rendering** -- `font_5x7_pixel()` returns individual pixels
   from the bitmap font, which are set in the SSD1306 framebuffer via
   `oled_ssd1306_set_pixel()`.

## Adapting for Your MCU

Replace HAL stubs:

- `hal_bmp_write_reg` / `hal_bmp_read_regs` -- I2C for BMP280
- `hal_oled_write_cmd` -- I2C command byte (control byte 0x00)
- `hal_oled_write_data` -- I2C data block (control byte 0x40)
- `hal_delay_ms` -- blocking millisecond delay

## Syntax Check

```bash
gcc -std=c99 -Wall -Wextra -fsyntax-only -I../../tooling/harness main.c
```

Designed for any Cortex-M with I2C. Tested in host simulation
(syntax-only -- HAL stubs are no-ops).
