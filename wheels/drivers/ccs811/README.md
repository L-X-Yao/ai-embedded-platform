# ccs811

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for AMS/ScioSense CCS811 digital eCO2 and TVOC gas
sensor. I2C interface injected via callbacks. Supports drive mode selection
(idle, 1sec, 10sec, 60sec). Handles boot-to-app mode transition.

## Origin

Proactive build. CCS811 is the most common indoor air quality sensor in
embedded IoT applications (HVAC, smart home, environmental monitoring).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Adafruit/SparkFun libraries are Arduino-specific with platform dependencies.
- No suitable zero-dependency C99 implementation found matching wheel contract.

Reference: AMS CCS811 datasheet (AS000000_CCS811).

## Dependencies

I2C HAL (injected via `ccs811_i2c_t` callbacks). No other dependencies.

## API

```c
ccs811_err_t ccs811_init(ccs811_t *dev, const ccs811_i2c_t *i2c,
                         const ccs811_cfg_t *cfg);
ccs811_err_t ccs811_read(ccs811_t *dev, ccs811_data_t *out);
ccs811_err_t ccs811_set_env_data(ccs811_t *dev, float humidity_pct,
                                 float temperature_c);
```

Output: `ccs811_data_t { uint16_t eco2_ppm; uint16_t tvoc_ppb; }`

## Configuration (compile-time)

None. Drive mode and I2C address are runtime-configurable via `ccs811_cfg_t`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-11
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- CCS811 requires 20-minute warm-up for accurate readings after first power-on.
- Readings in first 15 seconds after mode change may be unreliable.
- set_env_data sends 4 individual byte writes (HAL limitation).

## How to test

```bash
cd wheels/drivers/ccs811/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ccs811
```

28 assertions across 15 test cases + 7 hardware model tests + 6 null tests.

## Test status

- Unit tests (sim): PASS (15 C tests, 7 hw_model tests, 6 null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-11
- **Key decisions**: Init handles boot-to-app transition (APP_START write); ERR_NO_DATA returned when DATA_READY not set (caller polls); env compensation via separate API call.

## Responsible human

@user
