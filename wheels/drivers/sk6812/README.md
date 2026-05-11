# sk6812 -- WorldSemi SK6812 RGBW addressable LED driver

**Status: candidate**
**Layer: L1 (drivers)**

RGBW LED driver for SK6812 addressable LEDs. 4 bytes per pixel in GRB+W wire
order. Timing-critical data transmission delegated to platform-specific HAL
callback. Provides pixel buffer manipulation (set/fill/clear) and show.

## Origin

Proactive build for LED lighting projects. SK6812 is the RGBW variant of
the popular WS2812 protocol family.

## Why self-written?

Self-written following ws2812 wheel pattern. Adds white channel support.

## Dependencies

- `gpio_hal` (callback injection, no concrete dependency)

## How to test

```bash
cd wheels/drivers/sk6812/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sk6812
```

## Supported platforms

- [x] Host (sim)
- [ ] Real hardware -- not yet verified

## Responsible human

@user