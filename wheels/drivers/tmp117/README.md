# tmp117 -- TI TMP117 high-accuracy digital temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the TI TMP117 high-accuracy digital
temperature sensor. I2C interface injected via callbacks. 16-bit
resolution (0.0078125 deg C/LSB). Accuracy is +/-0.1 deg C maximum from
-20 to 50 deg C. Configurable hardware averaging (none, 8, 32, or 64
samples).

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

## Datasheet check

Checked on 2026-11-06 against TI SNOSD82D (September 2022). Not run on hardware.

- **Matched:** Addresses, register addresses, the device ID, the temperature format, the AVG and MOD fields, Soft_Reset and its 2 ms, 16-bit access.
- **Fixed:** `tmp117_read` returned -256 C with `TMP117_OK` until the first conversion finished, and now returns `TMP117_ERR_NOT_READY`. `tmp117_init` compared the revision bits too, so a later revision would have failed with `TMP117_ERR_ID`. The header called +/-0.1 C typical, where the manual gives it as a maximum from -20 C to 50 C. The model's power-up comment now gives 8 averages. Soft_Reset reloads the reset values and reads back 0, read-only registers and bits ignore writes, and both reset tests check that effect.
- **Still open:** There is no alert or therm mode, no one-shot or shutdown mode, no conversion cycle setting, and no EEPROM, offset or unique ID access. `tmp117_reset` does not reapply `cfg.avg`, so the sensor goes back to its EEPROM defaults.
- **Suspected:** `tmp117_init` writes only AVG, so CONV falls to 000 and the sensor converts almost all the time, with more current and self-heating. It also writes the configuration without waiting for the POR load or checking EEPROM_Busy, so the setting can be lost right after power-up.

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

@L-X-Yao
