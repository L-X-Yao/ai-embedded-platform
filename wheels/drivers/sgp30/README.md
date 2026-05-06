# sgp30 — Sensirion SGP30 air quality sensor driver (TVOC + eCO2)

**Status: candidate**
**Layer: L1 (drivers)**

I2C driver for the Sensirion SGP30 multi-pixel gas sensor. Provides TVOC
(0..60000 ppb) and eCO2 (400..60000 ppm) measurements. Supports baseline
save/restore for persistent calibration, humidity compensation, and serial
number readout. CRC-8 verified on all responses.

## Origin

Proactive build for indoor air quality monitoring applications (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- Sensirion official `embedded-sgp` driver requires their `embedded-common` HAL layer (not zero-dependency).
- Adafruit_SGP30 is C++/Arduino, not bare C99.
- No suitable zero-dependency C99 implementation found matching wheel contract.

Reference: Sensirion SGP30 datasheet v1.0 (command table, CRC spec, timing).

## Dependencies

None. I2C HAL injected via callbacks.

## API

```c
sgp30_err_t sgp30_init(sgp30_t *dev, const sgp30_i2c_t *i2c);
sgp30_err_t sgp30_measure_iaq(sgp30_t *dev, sgp30_data_t *out);
sgp30_err_t sgp30_get_baseline(sgp30_t *dev, sgp30_baseline_t *out);
sgp30_err_t sgp30_set_baseline(sgp30_t *dev, const sgp30_baseline_t *bl);
sgp30_err_t sgp30_get_serial_id(sgp30_t *dev, sgp30_serial_t *out);
sgp30_err_t sgp30_set_humidity(sgp30_t *dev, uint16_t abs_humidity_8_8);
uint8_t     sgp30_crc8(const uint8_t *data, uint16_t len);
```

## Configuration (compile-time)

None. All configuration is runtime via `sgp30_i2c_t` callback injection.

## Supported platforms

- [x] Host (sim) — 2026-05-06
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/sgp30/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sgp30
./build/test_null_sgp30
```

## Test status

- Unit tests (sim): 38 PASS (main) + 12 PASS (null) = 50 total
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Raw write/read I2C HAL (not register-based) to match SGP30's 16-bit command protocol. Same CRC-8 as SHT series. Returns integer ppb/ppm (no float conversion needed).

## Responsible human

@user
