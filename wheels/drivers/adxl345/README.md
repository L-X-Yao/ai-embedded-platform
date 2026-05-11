# adxl345

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Analog Devices ADXL345 3-axis digital
accelerometer. I2C interface injected via callbacks. Full-resolution mode
with configurable range (2g/4g/8g/16g). Scale: 3.9 mg/LSB.

## Origin

Proactive build. ADXL345 is one of the most widely used accelerometers
in embedded applications (robotics, motion detection, tilt sensing).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Analog Devices provides official driver but it has platform dependencies.
- No suitable zero-dependency C99 implementation found matching wheel contract.

Reference: Analog Devices ADXL345 datasheet (Rev. G).

## Dependencies

I2C HAL (injected via `adxl345_i2c_t` callbacks). No other dependencies.

## API

```c
adxl345_err_t adxl345_init(adxl345_t *dev, const adxl345_i2c_t *i2c,
                           const adxl345_cfg_t *cfg);
adxl345_err_t adxl345_read(adxl345_t *dev, adxl345_data_t *out);
```

Output: `adxl345_data_t { float x_g; float y_g; float z_g; }` (gravitational units)

## Configuration (compile-time)

None. Range and I2C address are runtime-configurable via `adxl345_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Uses full-resolution mode only. Non-full-res (fixed 10-bit) not exposed.
- Only I2C interface; ADXL345 also supports SPI.

## How to test

```bash
cd wheels/drivers/adxl345/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_adxl345
```

30 assertions across 16 test cases + 9 hardware model tests + 5 null tests.

## Test status

- Unit tests (sim): PASS (16 C tests, 9 hw_model tests, 5 null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: Full-resolution mode (3.9mg/LSB all ranges) for simplicity; range in config struct; output in g units (float).

## Responsible human

@user
