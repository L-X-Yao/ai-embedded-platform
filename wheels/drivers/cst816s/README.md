# cst816s — Hynitron CST816S capacitive touch controller (I2C)

**Status: candidate**
**Layer: L1 (drivers)**

Hynitron CST816S single-point capacitive touch controller for small displays
(smartwatches, wearables). I2C address 0x15, chip ID 0xB5. Provides touch
coordinates, event type (down/up/contact), and gesture recognition (swipe,
tap, double-tap, long-press).

## Origin

Proactive build for display-equipped wearable/IoT projects requiring touch input.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- pine64/InfiniTime CST816S driver: tightly coupled to NRF52 HAL and C++.
- sqfmi/Watchy: Arduino-specific, not portable C99.
- Various ESP-IDF components: framework-locked, not zero-dependency.
- No suitable zero-dependency C99 implementation found.

Reference: CST816S datasheet (Hynitron).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
cst816s_err_t cst816s_init(cst816s_t *dev, const cst816s_i2c_t *i2c);
cst816s_err_t cst816s_read(cst816s_t *dev, cst816s_touch_t *out);
cst816s_err_t cst816s_set_auto_sleep(cst816s_t *dev, bool enable);
cst816s_err_t cst816s_set_motion_mask(cst816s_t *dev,
                                      bool continuous,
                                      bool double_click,
                                      bool long_press);
uint8_t cst816s_get_chip_id(const cst816s_t *dev);
uint8_t cst816s_get_firmware_version(const cst816s_t *dev);
```

## Configuration (compile-time)

No compile-time defines. All configuration via runtime API.

## Supported platforms

- [x] Host (sim) — 2026-05-12
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/cst816s/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_cst816s
./build/test_hw_model_cst816s
```

## Test status

- Unit tests (sim): 19 pass (mock) + 12 pass (hw model) = 31 total
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: Single-point only (CST816S hardware limit). Gesture enum
  values match raw register IDs for zero-cost mapping. Chip ID and firmware
  version cached during init to avoid repeated I2C reads.

## Responsible human

@user
