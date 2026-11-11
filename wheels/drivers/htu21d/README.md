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

## Datasheet check

Checked on 2026-11-06 against HPC199_3 HTU21D(F) Sensor Datasheet, October 2013, revision 3. Not run on hardware.

- **Matched:** Address, commands, the soft reset and its 15 ms, the 50 ms measurement wait, the 3-byte frame, status bit masking, the RH and temperature formulas, CRC-8 on the manual's examples.
- **Fixed:** `htu21d_init` sent its first command with no wait, although the sensor can take 15 ms after power-up to accept one. It now waits first, so init takes 30 ms. The model now sets status bit 1 in humidity frames, and the tests use the manual's frame and CRC examples.
- **Still open:** There is no user register access, so resolution, heater and End of Battery are out of reach, and a heater left on stays on because soft reset keeps that bit. There is no RH temperature compensation, which the manual gives for its stated accuracy from 0 to 80 C. Status bit 1 is not checked, so a temperature frame read as humidity goes unnoticed. The reference above cites an August 2014 datasheet, which this check did not use.
- **Suspected:** The driver sends the Hold Master commands but follows the No Hold flow of wait, then read. Every wait is skipped when `delay_ms` is NULL.

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

- Unit tests (sim): 18 tests (mock) + 17 tests (hw_model)
- HIL: N/A

## API convention deviations

Uses `write_cmd`/`read_data` HAL instead of `write_reg`/`read_regs` because HTU21D is command-based (no register addresses).

## API review

- **Date**: 2026-05-13
- **Key decisions**: Separate read_temperature/read_humidity (not combined) to match command protocol. CRC-8 exposed as public for testing.

## Responsible human

@L-X-Yao
