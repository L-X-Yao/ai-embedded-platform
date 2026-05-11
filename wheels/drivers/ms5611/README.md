# ms5611

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for TE Connectivity MS5611 precision barometric
pressure sensor. I2C command-based protocol injected via callbacks.
Implements full 2nd-order temperature compensation from the datasheet.

## Origin

Proactive build. MS5611 is the standard barometric sensor for drones,
altimeters, and weather stations (24-bit ADC, 10 cm altitude resolution).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Most MS5611 libraries are Arduino/platform-specific or use blocking delays.
- No suitable zero-dependency C99 implementation found matching wheel contract.

Reference: TE Connectivity MS5611-01BA03 datasheet (DA5611-01BA03_011).

## Dependencies

I2C HAL (injected via `ms5611_i2c_t` callbacks). delay_ms required for
conversion wait. No other dependencies.

## API

```c
ms5611_err_t ms5611_init(ms5611_t *dev, const ms5611_i2c_t *i2c,
                         const ms5611_cfg_t *cfg);
ms5611_err_t ms5611_read(ms5611_t *dev, ms5611_data_t *out);
```

Output: `ms5611_data_t { float temperature_c; float pressure_pa; }`

## Configuration (compile-time)

None. OSR and I2C address are runtime-configurable via `ms5611_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Command-based protocol adapted to write_reg/read_regs HAL (reg byte used as
  command byte, val=0 for commands).
- No CRC-4 check on PROM data (datasheet optional).

## How to test

```bash
cd wheels/drivers/ms5611/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ms5611
```

23 assertions across 11 test cases + 7 hardware model tests + 5 null tests.

## Test status

- Unit tests (sim): PASS (11 C tests, 7 hw_model tests, 5 null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: PROM read at init; blocking conversion with delay_ms callback; 2nd-order compensation for accuracy below 20C; OSR configurable (256..4096).

## Responsible human

@user
