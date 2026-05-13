# htu21d — TE HTU21D digital humidity and temperature sensor

**Status: candidate**
**Layer: L1 (drivers)**

Command-based I2C driver for the TE Connectivity HTU21D humidity and temperature sensor. Fixed address 0x40, 14-bit temperature, 12-bit humidity, CRC-8 protected responses. Pure C99, no heap, fully reentrant.

## Origin

Proactive build for environmental sensing use cases (ADR-011 ecosystem-driven).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- Existing HTU21D libraries are Arduino/C++ or tightly coupled to specific HALs (e.g., STM32 HAL, ESP-IDF).
- No suitable zero-dependency C99 implementation found that matches wheel contract.

Reference: TE HTU21D datasheet (August 2014 revision).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
htu21d_err_t htu21d_init(htu21d_t *dev, const htu21d_i2c_t *i2c);
htu21d_err_t htu21d_read_temperature(htu21d_t *dev, float *temp_c);
htu21d_err_t htu21d_read_humidity(htu21d_t *dev, float *rh_percent);
htu21d_err_t htu21d_reset(htu21d_t *dev);
uint8_t      htu21d_crc8(const uint8_t *data, uint16_t len);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `CONTRACT_ENABLED` | off | Enable REQUIRE/ENSURE runtime checks |

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/htu21d/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_htu21d
./build/test_hw_model_htu21d
```

## Test status

- Unit tests (sim): 17 tests (mock) + 17 tests (hw_model)
- HIL: N/A

## API convention deviations

Uses `write_cmd`/`read_data` HAL instead of `write_reg`/`read_regs` because HTU21D is command-based (no register addresses).

## API review

- **Date**: 2026-05-13
- **Key decisions**: Separate read_temperature/read_humidity (not combined) to match command protocol. CRC-8 exposed as public for testing.

## Responsible human

@user
