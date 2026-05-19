# adxl345

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Analog Devices ADXL345 3-axis digital
accelerometer. I2C interface injected via callbacks. Full-resolution mode
provides 3.9 mg/LSB regardless of range setting.

## Origin

Proactive build (ADR-011). ADXL345 is one of the most widely used MEMS
accelerometers in embedded applications (drones, robotics, tilt sensing).

## Why self-written?

Searched GitHub (2026-05-11). No single-file zero-dependency C99 driver
exists that matches our wheel contract. Self-written for platform
convention consistency; register sequences from ADXL345 datasheet (Rev. E).

## Dependencies

I2C HAL (injected via `adxl345_i2c_t` callbacks). No other dependencies.

## API

```c
adxl345_err_t adxl345_init(adxl345_t *dev, const adxl345_i2c_t *i2c,
                           const adxl345_cfg_t *cfg);
adxl345_err_t adxl345_read(const adxl345_t *dev, adxl345_data_t *out);
adxl345_err_t adxl345_set_range(adxl345_t *dev, adxl345_range_t range);
```

Output: `adxl345_data_t { float x_g; float y_g; float z_g; }`

## Configuration (compile-time)

None. Range, data rate, and I2C address are runtime-configurable via `adxl345_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Only I2C is implemented (ADXL345 also supports SPI).
- FIFO and interrupt features not implemented.

## How to test

```bash
cd wheels/drivers/adxl345/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_adxl345
```

## Test status

- Unit tests (sim): PASS
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
