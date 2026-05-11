# apa102 — APA102 (DotStar) SPI-based addressable RGB LED driver

**Status: candidate**
**Layer: L1 (drivers)**

APA102/DotStar SPI-based addressable RGB LED strip driver. Each LED has independent
5-bit brightness control (0-31) plus 8-bit RGB. Pixel buffer is caller-provided.

## Origin

Proactive build for LED strip use cases (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Adafruit DotStar library is Arduino/C++ dependent.
- No suitable zero-dependency C99 implementation found.

Reference: APA102 datasheet (Shenzhen LED Color Opto-Electronic).

## Dependencies

None (SPI HAL injected via function pointers).

## API

```c
apa102_err_t apa102_init(apa102_t *dev, const apa102_spi_t *spi,
                         uint16_t num_leds);
apa102_err_t apa102_set_pixel(apa102_pixel_t *buf, uint16_t num_leds,
                              uint16_t index, uint8_t r, uint8_t g,
                              uint8_t b, uint8_t brightness);
apa102_err_t apa102_show(apa102_t *dev, const apa102_pixel_t *buf);
void         apa102_clear(apa102_pixel_t *buf, uint16_t num_leds);
```

## Supported platforms

- [x] Host (sim) — 2026-05-11
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/apa102/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_apa102 && ./build/test_hw_model_apa102 && ./build/test_null_apa102
```

## Test status

- Unit tests (sim): 26 passed (16 unit + 4 hw_model + 6 null)
- HIL: N/A

## API convention deviations

None.

## API Review

Pending.

## Responsible human

@user
