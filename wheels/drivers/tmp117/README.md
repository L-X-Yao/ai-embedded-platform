# tmp117 -- TI TMP117 high-accuracy digital temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the TI TMP117 high-accuracy digital
temperature sensor. I2C interface injected via callbacks. 16-bit
resolution (0.0078125 deg C/LSB) with typical accuracy of +/-0.1 deg C.
Configurable hardware averaging (none, 8, 32, or 64 samples).

## Origin

Proactive build. TMP117 is a precision temperature sensor for medical,
laboratory, and industrial calibration applications.

## Why self-written?

No existing open-source component for TMP117. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `tmp117_i2c_t` callbacks). No other dependencies.

## API

```c
tmp117_err_t tmp117_init(tmp117_t *dev, const tmp117_i2c_t *i2c,
                         const tmp117_cfg_t *cfg);
tmp117_err_t tmp117_read(tmp117_t *dev, float *temp_c);
tmp117_err_t tmp117_reset(tmp117_t *dev);
```

## Usage

```c
tmp117_t sensor;
tmp117_cfg_t cfg = { .addr = TMP117_ADDR_GND,
                     .avg = TMP117_AVG_8 };
tmp117_init(&sensor, &i2c_hal, &cfg);

float temp;
tmp117_read(&sensor, &temp);
// temp in degrees Celsius
```

## Configuration (compile-time)

None. I2C address and averaging mode are runtime-configurable via
`tmp117_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Alert/threshold mode not implemented.
- One-shot conversion mode not implemented.

## How to test

```bash
cd wheels/drivers/tmp117/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tmp117
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

@user
