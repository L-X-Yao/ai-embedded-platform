# ccs811

**Status: candidate**
**Layer: L1 (drivers)**

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

## Responsible human
@user
