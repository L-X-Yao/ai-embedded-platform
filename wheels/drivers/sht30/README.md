# sht30

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Sensirion SHT30/SHT31/SHT35 digital temperature
and humidity sensors. I2C interface injected via callbacks. CRC-8 verified
responses per Sensirion protocol specification.
## Origin

Proactive build. SHT3x is the most widely used digital humidity sensor family
in embedded applications (HVAC, weather stations, indoor air quality, IoT).

## Dependencies

I2C HAL (injected via `sht30_i2c_t` callbacks). No other dependencies.

Note: SHT30 uses 16-bit command words over I2C (not register-based), so the
HAL callbacks differ from typical register-based sensors like BMP280.

## API

```c
sht30_err_t sht30_init(sht30_t *dev, const sht30_i2c_t *i2c,
                       const sht30_cfg_t *cfg);
sht30_err_t sht30_read(sht30_t *dev, sht30_data_t *out);
sht30_err_t sht30_reset(sht30_t *dev);
uint8_t     sht30_crc8(const uint8_t *data, uint16_t len);
```

Output: `sht30_data_t { float temperature_c; float humidity_pct; }`

## Configuration (compile-time)

None. I2C address is runtime-configurable via `sht30_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-04-29
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Known issues

- Uses single-shot high-repeatability mode. No continuous (periodic) mode yet.
- No clock-stretching timeout handling — relies on I2C HAL.

## How to test

```bash
cd wheels/drivers/sht30/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_sht30
```

28 assertions across 15 test cases: init with soft reset, init I2C fail, null
safety (6 paths), read 25°C/50% verified, read 0°C/0%, read -45°C boundary,
CRC fail temperature, CRC fail humidity, I2C fail on command, I2C fail on data
read, reset command, reset I2C fail, alternate address (0x45), CRC-8 known
vector (0xBEEF → 0x92), measure command encoding.

## Test status

- Unit tests (sim): PASS (15 C tests, 28 assertions)
- HIL: N/A

## API convention deviations

I2C HAL uses raw write/read callbacks instead of register-based write_reg/read_regs,
because SHT30 uses 16-bit command words rather than 8-bit register addresses.

## API review

- **Date**: 2026-04-29
- **Key decisions**: CRC-8 verification mandatory (no skip option) for data integrity; soft reset on init to ensure known state; sht30_crc8 exposed as public API for use in custom command sequences; temperature formula per datasheet: T = -45 + 175 * raw / 65535.

## Responsible human
@user
