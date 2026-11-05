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

## Datasheet check

Checked on 2026-11-06 against the Shenzhen LED Color Opto Electronic APA102 datasheet, which prints no document number or revision. The frame figure on p4 is an image, so it was read from the rendered page. Not run on hardware.

- **Matched:** Start frame, LED frame header and BGR byte order, brightness 0-31, 8 bits per colour, an end frame of at least 32 one-bits.
- **Still open:** VEN self-detection is not supported, which matters only where VEN is strapped on a bare IC. The HAL contract is unwritten: bits must go MSB first. `cs_low` and `cs_high` are mandatory although the part has no chip select, so a caller must supply stubs.
- **Suspected:** `apa102_show` sends ceil(N/2) end-frame bytes, more than the manual's 32 bits above 8 LEDs, which looks like a slip of bytes for bits and costs about 10% more bus time on long strips. The model passes a frame with no end frame.

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

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
