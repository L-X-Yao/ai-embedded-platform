# aht20 — ASAIR AHT20 temperature and humidity sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for ASAIR AHT20 digital temperature and humidity
sensor. I2C interface injected via callbacks. CRC-8 verified responses
(poly 0x31, init 0xFF). Handles calibration check and init sequence per
datasheet. Successor to AHT10, extremely popular in Chinese IoT market.

## Origin

Proactive build. AHT20 is one of the most widely used low-cost temperature
and humidity sensors in Chinese embedded/IoT applications.

## Why self-written?

Searched GitHub (2026-05-07). Referenced ASAIR AHT20 datasheet for register
sequences, timing requirements, and compensation algorithms. Self-written for
platform convention consistency (zero-dependency, injected HAL, no heap).

## Dependencies

I2C HAL (injected via `aht20_i2c_t` callbacks). No other dependencies.

Note: AHT20 uses command-based protocol over I2C (not register-based), so the
HAL callbacks use raw write/read instead of write_reg/read_regs.

## API

```c
aht20_err_t aht20_init(aht20_t *dev, const aht20_i2c_t *i2c);
aht20_err_t aht20_read(aht20_t *dev, aht20_data_t *out);
aht20_err_t aht20_reset(aht20_t *dev);
bool        aht20_is_calibrated(aht20_t *dev);
uint8_t     aht20_crc8(const uint8_t *data, uint16_t len);
```

Output: `aht20_data_t { float temperature_c; float humidity_pct; }`

## Configuration (compile-time)

None. I2C address is fixed at 0x38 (hardware limitation).

## Supported platforms

- [x] Host (sim) — passed 2026-05-07
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against ASAIR AHT20 Product manuals, Version V1.1, 2020/04. Not run on hardware.

- **Matched:** Address, the init, trigger and soft reset commands, status bits 7 and 3, the calibration check, the waits, frame order, CRC, both conversion formulas.
- **Fixed:** When the sensor was still busy after 80 ms, `aht20_read` gave up with `AHT20_ERR_I2C` or `AHT20_ERR_CRC`. It now reads again for up to 200 ms more, then returns the new `AHT20_ERR_TIMEOUT`. A NULL `delay_ms` removed every wait, so `aht20_init` now returns `AHT20_ERR_PARAM`. No test used `hw_model_aht20.h`, which was never busy, filled reads of 2 to 6 bytes with FFh and turned 100 %RH into a raw value that wrapped to 0. It now follows the waits and the frame of the manual, and `test_hw_model_aht20.c` runs the driver against it.
- **Still open:** The manual does not say which nibble of the shared byte is humidity, so the split here is the usual one and unproven. Nothing limits the rate, though the manual recommends one measurement every 2 seconds to keep self-heating low.

## Known issues

- Single-shot measurement mode only. No continuous/periodic mode.
- No clock-stretching timeout handling — relies on I2C HAL.
- Fixed I2C address (0x38) — no multi-sensor support on same bus.

## How to test

```bash
cd wheels/drivers/aht20/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_aht20
./build/test_null_aht20
```

31 assertions across 24 + 7 test cases: init (already calibrated, needs
calibration, I2C fail on status read / init cmd / second status read), null
safety (6 paths), null callback validation, read 25C/50%, read 0C/0%, read
-50C boundary, read 100% humidity, CRC fail, busy bit error, I2C fail on
trigger / data read, trigger command encoding, reset command, reset I2C fail,
calibration status tracking, CRC-8 known vector (0xBEEF -> 0x92), CRC null
input, CRC empty input, error code values.

## Test status

- Unit tests (sim): PASS (24 C tests + 7 null tests, 31 total)
- HIL: N/A

## API convention deviations

I2C HAL uses raw write/read callbacks instead of register-based write_reg/read_regs,
because AHT20 uses command-based protocol rather than register addresses.

## API review

- **Date**: 2026-05-07
- **Key decisions**: CRC-8 verification mandatory for data integrity; calibration check on init with automatic init command if needed; soft reset exposed as public API; aht20_is_calibrated checks cached status byte; temperature formula per datasheet: T = raw/2^20 * 200 - 50; humidity formula: H = raw/2^20 * 100.

## Responsible human
@L-X-Yao
