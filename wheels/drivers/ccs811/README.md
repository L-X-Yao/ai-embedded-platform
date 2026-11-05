# ccs811

**Status: candidate**
**Layer: L1 (drivers)**

> **Not usable on hardware yet.** The HAL cannot send the address-only write that starts the application, and `ccs811_set_env_data` writes the wrong registers. See Datasheet check.

Platform-agnostic driver for AMS CCS811 indoor air quality sensor.
I2C interface injected via callbacks. Reads eCO2 (400-8192 ppm) and
TVOC (0-1187 ppb). Implements boot sequence and drive mode configuration.

## Origin

Proactive build (ADR-011). CCS811 is a popular VOC/eCO2 sensor for
indoor air quality monitoring in HVAC and smart home applications.

## Why self-written?

Searched GitHub (2026-05-11). No single-file zero-dependency C99 driver
matching our wheel contract. Self-written; protocol from CCS811 datasheet.

## Dependencies

I2C HAL (injected via `ccs811_i2c_t` callbacks). No other dependencies.

## API

```c
ccs811_err_t ccs811_init(ccs811_t *dev, const ccs811_i2c_t *i2c,
                         const ccs811_cfg_t *cfg);
ccs811_err_t ccs811_read(const ccs811_t *dev, ccs811_data_t *out);
ccs811_err_t ccs811_set_mode(ccs811_t *dev, ccs811_drive_mode_t mode);
ccs811_err_t ccs811_set_env_data(const ccs811_t *dev, uint16_t humidity,
                                 uint16_t temperature);
```

Output: `ccs811_data_t { uint16_t eco2_ppm; uint16_t tvoc_ppb; }`

## Configuration (compile-time)

None. Drive mode and I2C address are runtime-configurable via `ccs811_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against ams CCS811 datasheet v1-00. Not run on hardware.

- **Matched:** I2C addresses, HW_ID, register addresses, STATUS bits, drive mode codes, result byte order, the APP_VALID and FW_MODE checks.
- **Fixed:** `ccs811_read` returned old values in the 250 ms raw mode, where the chip leaves ALG_RESULT_DATA alone. It now returns `CCS811_ERR_MODE`.
- **Still open:** The HAL has no address-only write, so APP_START goes out with a data byte the manual does not allow. It has no multi-byte write either, so `ccs811_set_env_data` sends two single bytes, one to a register that does not exist, and its units disagree with the manual. Both need HAL changes. `hw_model_ccs811.h` is not compiled by any test and does not model the mailbox registers.
- **Suspected:** A change to a slower mode should pass through idle for 10 minutes. The STATUS error bit is never read. APP_START is sent even when the chip is already in application mode. There is no wait after power-on.

## Known issues

- Environmental data compensation (set_env_data) writes only first byte
  due to single-byte write_reg HAL limitation. Full 4-byte write would
  need a write_regs HAL callback.
- Baseline save/restore not implemented.

## How to test

```bash
cd wheels/drivers/ccs811/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ccs811
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
