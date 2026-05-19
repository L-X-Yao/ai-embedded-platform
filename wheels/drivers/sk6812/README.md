# sk6812 — WorldSemi SK6812 RGBW addressable LED driver

**Status: candidate**
**Layer: L1 (drivers)**

RGBW addressable LED driver for SK6812. Handles pixel buffer management
(set/fill/clear) and sends data via HAL-injected send_data callback.
Timing is platform-specific; the HAL abstracts the bit-bang or DMA
implementation.

## Origin

Proactive build for LED lighting and display projects. SK6812 adds a
dedicated white channel vs. WS2812 (RGB only).

## Why self-written?

Complements existing `ws2812` wheel. SK6812 uses 4 bytes/LED (GRBW) vs.
WS2812's 3 bytes/LED (GRB). HAL abstracts timing so the driver focuses
on pixel buffer management.

## Dependencies

- `gpio_hal` (callback injection, no concrete dependency)

## API

```c
sk6812_err_t sk6812_init(sk6812_t *dev, const sk6812_hal_t *hal, uint16_t num_leds);
sk6812_err_t sk6812_set_pixel(sk6812_t *dev, sk6812_pixel_t *buf, uint16_t idx, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
sk6812_err_t sk6812_show(sk6812_t *dev, const sk6812_pixel_t *buf);
sk6812_err_t sk6812_fill(sk6812_t *dev, sk6812_pixel_t *buf, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
sk6812_err_t sk6812_clear(sk6812_t *dev, sk6812_pixel_t *buf);
```

## Configuration (compile-time)

None. Number of LEDs set at init time.

## Supported platforms

- [x] Host (sim) -- 2026-05-11
- [ ] Real hardware (SK6812 strip) -- not yet verified

## Known issues

- Pixel buffer is caller-provided (no internal allocation).
- Wire order is GRBW (matches struct layout for zero-copy send).

## How to test

```bash
cd wheels/drivers/sk6812/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sk6812
```

## Test status

- Unit tests (sim): 19 pass
- Null tests: 10 pass
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
