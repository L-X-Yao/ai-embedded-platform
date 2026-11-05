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

## Datasheet check

Checked on 2026-11-06 against Sensirion SGP30 Datasheet, Version 1.0, May 2020, D1. Not run on hardware.

- **Matched:** I2C address, the command codes in use, waits against the Table 10 maximum durations, word orders including TVOC first in set_baseline, response lengths, the 8.8 humidity format, CRC-8 (0xBEEF gives 0x92), iaq_init at init.
- **Fixed:** `sgp30_init` accepted a NULL `delay_ms` and every command then skipped its wait, so reads landed during the measurement, when the sensor aborts communication. It now refuses that HAL.
- **Still open:** The model has no busy period, serves the same response twice and fills unserved bytes with 0, where the sensor aborts during a measurement, allows one read and returns 1s past the end, so tests cannot see a missing wait or a double read. No measure_raw, get_feature_set, measure_test, soft reset or inceptive baseline, and no helper from temperature and RH to absolute humidity. Nothing marks the 15 s initialization phase, when readings stay at 400 ppm and 0 ppb.

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

- Unit tests (sim): 39 PASS (main) + 12 PASS (null) = 51 total
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Raw write/read I2C HAL (not register-based) to match SGP30's 16-bit command protocol. Same CRC-8 as SHT series. Returns integer ppb/ppm (no float conversion needed).

## Responsible human

@user
