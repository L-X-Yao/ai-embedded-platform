# oled_ssd1306

**Status: candidate**
**Layer: L1 (drivers)**

Framebuffer-based driver for SSD1306 128x64/128x32 OLED displays over I2C.
Caller provides a framebuffer, draws into it with pixel/clear/fill operations,
then flushes to display. I2C interface injected via callbacks.

## Why not VSF native?

Searched `vsf/source/service/`, `vsf/source/component/`, `vsf/source/utilities/` for:
`ssd1306`, `oled`, `display` — found LVGL port but no SSD1306 driver.

## Origin

Proactive build. SSD1306 is the most common small OLED display in embedded
prototyping and products (debug output, status display, simple UI).

## Dependencies

I2C HAL (injected via `oled_ssd1306_i2c_t` callbacks). No other dependencies.

Note: SSD1306 uses a control byte prefix (0x00 for commands, 0x40 for data),
so the HAL callbacks are `write_cmd` and `write_data` (not register-based).

## API

```c
oled_ssd1306_err_t oled_ssd1306_init(oled_ssd1306_t *dev,
                                     const oled_ssd1306_i2c_t *i2c,
                                     const oled_ssd1306_cfg_t *cfg,
                                     uint8_t *framebuf);
oled_ssd1306_err_t oled_ssd1306_flush(oled_ssd1306_t *dev);
void               oled_ssd1306_clear(oled_ssd1306_t *dev);
void               oled_ssd1306_set_pixel(oled_ssd1306_t *dev, uint8_t x, uint8_t y, bool on);
bool               oled_ssd1306_get_pixel(const oled_ssd1306_t *dev, uint8_t x, uint8_t y);
void               oled_ssd1306_fill(oled_ssd1306_t *dev, bool on);
```

Framebuffer size: `OLED_SSD1306_BUF_SIZE(width, height)` = 1024 bytes for 128x64.

## Configuration (compile-time)

None. Display dimensions and I2C address are runtime-configurable.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Known issues

- No text/font rendering built in — add a font wheel or use LVGL for rich UI.
- No SPI support — I2C only.

## Tests

```bash
cd wheels/drivers/oled_ssd1306/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_oled_ssd1306
```

31 assertions across 13 test cases: init with command sequence, init I2C fail,
null safety (5 paths), pixel set/get/clear round-trip, out-of-bounds ignored,
clear, fill on/off, flush sends commands + data, flush I2C fail, vertical bit
packing (8 pixels = 1 byte), multiple pixels same byte, invalid height rejected,
null operations no-crash.

## Test status

- Unit tests (sim): PASS (13 C tests, 31 assertions)
- HIL: N/A

## API convention deviations

I2C HAL uses write_cmd/write_data instead of write_reg/read_regs because SSD1306
is command-based (control byte 0x00/0x40 prefix), not register-based.

## API review

- **Date**: 2026-04-29
- **Key decisions**: Framebuffer-based (not streaming) for flexibility and double-buffering potential; caller provides framebuffer (no heap); pixel coordinates use hardware-native vertical page layout internally but expose (x, y) API; init sends full SSD1306 initialization sequence per datasheet; 128x32 supported via height parameter and COM pin config auto-adjust.

## Responsible human
@user
