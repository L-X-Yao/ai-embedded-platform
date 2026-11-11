# vl53l1x -- ST VL53L1X time-of-flight distance sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

> **Not usable on hardware yet.** The driver checks the chip ID and writes one register. It does not load the configuration ST's driver loads, and it reports every reading as valid.

Platform-agnostic driver for the ST VL53L1X time-of-flight (ToF) laser
distance sensor. I2C interface injected via callbacks (uses 16-bit register
addresses, unlike most I2C sensors).

## Origin

Proactive build. VL53L1X is widely used for obstacle detection, gesture
sensing, and proximity measurement in robotics and IoT.

## Why self-written?

No existing open-source component for VL53L1X. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


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

## Datasheet check

Checked on 2026-11-05 against Pololu `vl53l1x-arduino`, a port of ST's API. ST's datasheet has no register map. Not run on hardware.

- **Matched:** Register addresses, model and module IDs, I2C address, start value, result byte order.
- **Fixed:** Nothing changed: the gaps need ST's ultra-lite driver ported, not a patch.
- **Still open:** No default configuration, boot wait or timing budget in init. Range status, data ready and the interrupt clear are never used, so failed and stale readings return `VL53L1X_OK`. Start sets no inter-measurement period. Whether stop should write 0x00 or 0x80, and whether the result needs a gain correction, depends on which ST API is followed.

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

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@L-X-Yao
