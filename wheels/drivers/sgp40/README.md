# sgp40

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Sensirion SGP40 VOC raw signal sensor. Uses
I2C command-based protocol with CRC-8 verification (poly 0x31, init 0xFF).
Returns raw SRAW value (0-65535); VOC index calculation requires the
Sensirion VOC Algorithm library (out of scope for this driver).

## Origin

Proactive build. SGP40 is the current-generation Sensirion VOC sensor for
indoor air quality applications (HVAC, air purifiers, smart home).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Sensirion provides embedded-hal driver but with platform-specific I2C layer.
- No suitable zero-dependency C99 implementation found matching wheel contract.

Reference: Sensirion SGP40 datasheet (Version 1.1).

## Dependencies

I2C HAL (injected via `sgp40_i2c_t` callbacks). delay_ms required for
self-test and measurement timing. No other dependencies.

## API

```c
sgp40_err_t sgp40_init(sgp40_t *dev, const sgp40_i2c_t *i2c);
sgp40_err_t sgp40_measure_raw(sgp40_t *dev, uint16_t rh_ticks,
                              uint16_t t_ticks, sgp40_data_t *out);
uint8_t     sgp40_crc8(const uint8_t *data, uint16_t len);
```

Output: `sgp40_data_t { uint16_t sraw; }` (raw VOC signal, 0-65535)

Default compensation: rh_ticks=0x8000 (50% RH), t_ticks=0x6666 (25C).

## Configuration (compile-time)

None. I2C address is fixed at 0x59 (SGP40 has no configurable address).

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- Returns raw SRAW only. Full VOC index requires Sensirion's proprietary
  algorithm (sensirion/gas-index-algorithm on GitHub, separate integration).
- Command protocol adapted to write_reg/read_regs HAL (MSB as reg, LSB as val).
- Self-test takes 320ms.

## How to test

```bash
cd wheels/drivers/sgp40/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sgp40
```

26 assertions across 14 test cases + 7 hardware model tests + 5 null tests.

## Test status

- Unit tests (sim): PASS (14 C tests, 7 hw_model tests, 5 null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: Self-test on init (0x280E, expect 0xD400); CRC-8 mandatory on all reads; raw SRAW output only (VOC index is a separate concern); humidity/temperature compensation passed as tick values per Sensirion convention.

## Responsible human

@user
