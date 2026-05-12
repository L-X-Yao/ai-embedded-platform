# scd40 — Sensirion SCD40/SCD41 NDIR CO2 + temperature + humidity sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

NDIR-based CO2 sensor with on-chip temperature and humidity. I2C command-based
protocol (16-bit command words, CRC-8 protected data). Supports periodic
measurement (5 s interval), data-ready polling, altitude compensation,
serial number readout, and soft reset.

## Origin

Proactive build for indoor air quality / CO2 monitoring use cases (ADR-011 ecosystem-driven).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- Sensirion official `embedded-i2c-scd4x` — tied to their HAL abstraction (`sensirion_i2c.h`), heavy for our zero-dep pattern.
- No suitable zero-dependency C99 implementation found that follows our wheel contract (callback-injected I2C, no heap, no OS).

Reference: Sensirion SCD40 datasheet (command table, CRC-8 polynomial 0x31).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
scd40_err_t scd40_init(scd40_t *dev, const scd40_i2c_t *i2c);
scd40_err_t scd40_start_periodic(scd40_t *dev);
scd40_err_t scd40_stop_periodic(scd40_t *dev);
scd40_err_t scd40_read(scd40_t *dev, scd40_data_t *out);
scd40_err_t scd40_data_ready(scd40_t *dev, bool *ready);
scd40_err_t scd40_get_serial(scd40_t *dev, uint16_t serial[3]);
scd40_err_t scd40_set_altitude(scd40_t *dev, uint16_t meters);
scd40_err_t scd40_perform_reset(scd40_t *dev);
uint8_t     scd40_crc8(const uint8_t *data, uint16_t len);
```

Error codes: `SCD40_OK` (0), `SCD40_ERR_I2C` (-1), `SCD40_ERR_CRC` (-2), `SCD40_ERR_PARAM` (-3).

## Configuration (compile-time)

No compile-time configuration needed. I2C address is fixed at 0x62.

## Supported platforms

- [x] Host (sim) — 2026-05-12
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/scd40/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_scd40
./build/test_hw_model_scd40
```

## Test status

- Unit tests (sim): 13 PASS / 0 FAIL (test_scd40)
- Hardware model tests (sim): 13 PASS / 0 FAIL (test_hw_model_scd40)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: Same Sensirion command-based pattern as SHT30. CRC-8 shared polynomial. `scd40_data_t` uses `uint16_t co2_ppm` (raw from sensor) + float for temp/humidity. `delay_ms` uses `uint32_t` (SCD40 has longer delays than SHT30).

## Responsible human

@user
