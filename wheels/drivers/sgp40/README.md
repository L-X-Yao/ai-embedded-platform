# sgp40

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Sensirion SGP40 VOC raw signal sensor.
Command-based I2C with CRC-8 protection (poly 0x31, init 0xFF).
Outputs raw VOC signal (SRAW, 0-65535). Supports humidity/temperature
compensation for improved accuracy.

## Origin

Proactive build (ADR-011). SGP40 is Sensirion's latest-generation VOC
sensor, successor to SGP30, widely used in air quality monitoring.

## Why self-written?

Searched GitHub (2026-05-11). Sensirion provides an embedded-hal driver
(BSD-3) but it depends on their HAL abstraction. Self-written for zero
dependency; CRC and command protocol from SGP40 datasheet.

## Dependencies

I2C HAL (injected via `sgp40_i2c_t` raw write/read callbacks).
No other dependencies.

## API

```c
sgp40_err_t sgp40_init(sgp40_t *dev, const sgp40_i2c_t *i2c);
sgp40_err_t sgp40_measure_raw(sgp40_t *dev, sgp40_data_t *out);
sgp40_err_t sgp40_measure_raw_comp(sgp40_t *dev, sgp40_data_t *out,
                                   uint16_t humidity, uint16_t temperature);
uint8_t sgp40_crc8(const uint8_t *data, uint16_t len);
```

Output: `sgp40_data_t { uint16_t sraw_voc; }`

## Configuration (compile-time)

None. I2C address is fixed at 0x59.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against Sensirion SGP40 datasheet v1.2. Not run on hardware.

- **Matched:** I2C address, command codes, word and CRC format, wait times, default compensation words.
- **Fixed:** The self-test compared both result bytes although only the first counts, so a healthy sensor could fail `sgp40_init`. The comments called the default words compensation, which they turn off.
- **Still open:** `hw_model_sgp40.h` is not compiled by any test.
- **Suspected:** The first sample should be discarded when the Gas Index Algorithm is not used, and the driver does not say so.

## Known issues

- Uses raw write/read I2C HAL (different from register-based drivers).
  Caller must provide raw byte-level I2C transport.
- VOC Index algorithm (Sensirion proprietary) not included; output is raw
  SRAW signal. Use Sensirion's VOC Index algorithm library if needed.

## How to test

```bash
cd wheels/drivers/sgp40/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sgp40
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

Uses raw write/read I2C HAL instead of register-based write_reg/read_regs,
matching SGP40's command-based protocol (same pattern as sgp30 wheel).

## API Review

Pending.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
