# vl53l1x -- ST VL53L1X time-of-flight distance sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the ST VL53L1X time-of-flight (ToF) laser
distance sensor. I2C interface injected via callbacks (uses 16-bit register
addresses, unlike most I2C sensors). Continuous ranging mode with 16-bit
millimeter distance output, up to 4 meters range.

## Origin

Proactive build. VL53L1X is widely used for obstacle detection, gesture
sensing, and proximity measurement in robotics and IoT.

## Why not VSF native?

No VSF component for VL53L1X. Self-written for zero-dependency I2C HAL
injection and platform conventions.

## Dependencies

I2C HAL with 16-bit register addressing (injected via `vl53l1x_i2c_t`
callbacks). No other dependencies.

## API

```c
vl53l1x_err_t vl53l1x_init(vl53l1x_t *dev, const vl53l1x_i2c_t *i2c,
                            const vl53l1x_cfg_t *cfg);
vl53l1x_err_t vl53l1x_read_distance(vl53l1x_t *dev, uint16_t *dist_mm);
vl53l1x_err_t vl53l1x_start_ranging(vl53l1x_t *dev);
vl53l1x_err_t vl53l1x_stop_ranging(vl53l1x_t *dev);
```

## Usage

```c
vl53l1x_t tof;
vl53l1x_cfg_t cfg = { .addr = VL53L1X_ADDR_DEFAULT };
vl53l1x_init(&tof, &i2c_hal, &cfg);

vl53l1x_start_ranging(&tof);

uint16_t dist;
vl53l1x_read_distance(&tof, &dist);
// dist in millimeters

vl53l1x_stop_ranging(&tof);
```

## Configuration (compile-time)

None. I2C address is runtime-configurable via `vl53l1x_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- I2C HAL must support 16-bit register addresses (different from standard
  8-bit register HAL used by most other driver wheels).
- Timing budget and distance mode configuration not exposed.

## How to test

```bash
cd wheels/drivers/vl53l1x/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_vl53l1x
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## Responsible human

@user
