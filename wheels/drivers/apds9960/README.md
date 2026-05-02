# apds9960 — I2C proximity, ambient light, RGB color, and gesture sensor

**Status: candidate**
**Layer: L1 (drivers)**

Broadcom/Avago APDS-9960 driver. Supports proximity detection (8-bit), RGBC
color sensing (16-bit per channel), and simple 4-directional gesture recognition
(UP/DOWN/LEFT/RIGHT) via FIFO photodiode data. Fixed I2C address 0x39.
## Origin

Proactive build for proximity/gesture/color sensing use cases (e.g., touchless
UI, ambient light adaptation).


## Why self-written?

Searched GitHub/embedded ecosystem for existing implementations.
No suitable zero-dependency C99 library found for this use case.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
apds9960_err_t apds9960_init(apds9960_t *dev, const apds9960_i2c_t *i2c);
apds9960_err_t apds9960_enable_proximity(apds9960_t *dev);
apds9960_err_t apds9960_read_proximity(apds9960_t *dev, uint8_t *prox);
apds9960_err_t apds9960_enable_color(apds9960_t *dev);
apds9960_err_t apds9960_read_color(apds9960_t *dev, apds9960_color_t *color);
apds9960_err_t apds9960_enable_gesture(apds9960_t *dev);
apds9960_err_t apds9960_read_gesture(apds9960_t *dev, apds9960_gesture_t *gesture);
apds9960_err_t apds9960_disable_all(apds9960_t *dev);
```

## Configuration (compile-time)

None. All configuration is handled at runtime via the API.

## Supported platforms

- [x] Host (sim) -- 2026-05-01
- [ ] Real hardware target -- not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/apds9960/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_apds9960
```

## Test status

- Unit tests (sim): 19 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Fixed address (0x39, no config needed). Simple FIFO-based gesture detection using U/D and L/R delta comparison with threshold.

## Responsible human

@user
