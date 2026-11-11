# sht40 — Sensirion SHT40 temperature and humidity sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

I2C temperature and humidity sensor with CRC-8 protected responses.
Supports three measurement precisions (high/medium/low) with different
conversion times. Command-response protocol: send 1-byte command, wait,
read 6 bytes.

## Origin

Proactive build for common environmental sensing use case (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- Sensirion official driver (`embedded-sht`) is C but pulls in their HAL abstraction layer, incompatible with our zero-dependency wheel contract.
- No suitable zero-dependency C99 implementation found that matches our DI pattern.

Reference: Sensirion SHT4x datasheet (version 4, September 2023).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
sht40_err_t sht40_init(sht40_t *dev, const sht40_i2c_t *i2c, uint8_t addr);
sht40_err_t sht40_read_high(sht40_t *dev, sht40_data_t *out);
sht40_err_t sht40_read_medium(sht40_t *dev, sht40_data_t *out);
sht40_err_t sht40_read_low(sht40_t *dev, sht40_data_t *out);
sht40_err_t sht40_soft_reset(sht40_t *dev);
sht40_err_t sht40_read_serial(sht40_t *dev, uint32_t *serial);
uint8_t     sht40_crc8(const uint8_t *data, uint16_t len);
```

## Configuration (compile-time)

No compile-time configuration needed. Address selected at runtime (0x44 or 0x45).

## Supported platforms

- [x] Host (sim) -- 2026-05-06
- [ ] Real hardware target -- not yet verified

## Datasheet check

Checked on 2026-11-06 against Sensirion SHT4x datasheet v6.4. Not run on hardware.

- **Matched:** Addresses, commands, wait times, response format, CRC, both conversion formulas, serial number read.
- **Fixed:** A test used the wrong raw value for 50 %RH. `sht40_soft_reset` returned at once, so a measurement straight after it failed with `SHT40_ERR_I2C`. It now waits tSR, 1 ms. The model returned data on every read. It now stays busy for the Table 5 maximum, NACKs read headers until then, loses commands sent meanwhile and gives measurement data out once. This fix has not run on hardware.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/sht40/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sht40
./build/test_null_sht40
```

## Test status

- Unit tests (sim): 34 pass + 12 null tests pass (46 total)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Single-byte command protocol (not register-based like SHT30's 2-byte commands). Humidity clamped to 0-100 per SHT40 datasheet recommendation. Three precision levels exposed as separate functions rather than a config enum.

## Responsible human

@L-X-Yao
