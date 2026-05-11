# ms5611

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for TE Connectivity MS5611 precision barometric
pressure sensor. I2C interface injected via callbacks. Implements the
factory-calibrated compensation algorithm from the MS5611-01BA03 datasheet
including 2nd-order correction for low temperatures.

## Origin

Proactive build (ADR-011). MS5611 is a precision barometer widely used in
drones, altimeters, and weather stations for its 10 cm altitude resolution.

## Why self-written?

Searched GitHub (2026-05-11). No single-file zero-dependency C99 driver
matching our wheel contract. Self-written; compensation formulas from
MS5611-01BA03 datasheet.

## Dependencies

I2C HAL (injected via `ms5611_i2c_t` callbacks). No other dependencies.

## API

```c
ms5611_err_t ms5611_init(ms5611_t *dev, const ms5611_i2c_t *i2c,
                         const ms5611_cfg_t *cfg);
ms5611_err_t ms5611_read(ms5611_t *dev, ms5611_data_t *out);
ms5611_err_t ms5611_reset(ms5611_t *dev);
```

Output: `ms5611_data_t { float temperature_c; float pressure_pa; }`

## Configuration (compile-time)

None. OSR and I2C address are runtime-configurable via `ms5611_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Command-based protocol uses write_reg/read_regs as transport; actual
  MS5611 protocol is different from standard register I2C.
- Only I2C is implemented (MS5611 also supports SPI).

## How to test

```bash
cd wheels/drivers/ms5611/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ms5611
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## Responsible human
@user
