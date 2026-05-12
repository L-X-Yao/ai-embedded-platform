# bme688 — Bosch BME688 environmental sensor driver (temp/hum/pres/gas)

**Status: candidate**
**Layer: L1 (drivers)**

Bosch BME688 temperature + pressure + humidity + gas resistance (VOC) sensor
driver. I2C interface injected via callbacks. Implements Bosch compensation
algorithms (integer arithmetic for TPH, lookup-table for gas resistance).
Register-compatible with BME680 (same chip ID 0x61).

## Origin

Proactive build for air quality monitoring use cases (ADR-011 ecosystem-driven).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-12):

- Bosch BSEC library: closed-source binary blob, not vendor-able, platform-locked.
- Bosch BME68x-Sensor-API: BSD-licensed but tightly coupled to their config struct
  and requires their platform adaptation layer. Too much baggage for a zero-dep wheel.
- No suitable zero-dependency C99 implementation found that matches our wheel contract.

Reference: BME688 datasheet BST-BME688-DS000-18, BME680 datasheet BST-BME680-DS001-00.

## Dependencies

None. I2C HAL injected via function pointers.

## API

```c
bme688_err_t bme688_init(bme688_t *dev, const bme688_i2c_t *i2c,
                         const bme688_cfg_t *cfg);
bme688_err_t bme688_read(bme688_t *dev, bme688_data_t *out);
bme688_err_t bme688_set_gas_heater(bme688_t *dev, uint16_t temp_c,
                                   uint16_t dur_ms);
bme688_err_t bme688_reset(bme688_t *dev);
```

Error codes: `BME688_OK`, `BME688_ERR_HAL`, `BME688_ERR_PARAM`, `BME688_ERR_ID`, `BME688_ERR_TIMEOUT`.

## Configuration (compile-time)

No compile-time defines. All configuration is runtime via `bme688_cfg_t`:

| Field              | Range    | Purpose                         |
|--------------------|----------|---------------------------------|
| osrs_t             | 0-5      | Temperature oversampling        |
| osrs_p             | 0-5      | Pressure oversampling           |
| osrs_h             | 0-5      | Humidity oversampling           |
| filter             | 0-7      | IIR filter coefficient          |
| gas_heater_temp_c  | 200-400  | Gas heater target temperature   |
| gas_heater_dur_ms  | 1-4032   | Gas heater duration             |
| addr               | 0x76/77  | I2C address                     |

## Supported platforms

- [x] Host (sim) — 2026-05-12
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/bme688/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bme688
./build/test_hw_model_bme688
```

## Test status

- Unit tests (sim): 34 pass, 0 fail
- HW model tests (sim): 13 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: BME688 uses same register map as BME680 (chip ID 0x61).
  API adds `bme688_cfg_t` for init-time configuration (oversampling, filter,
  gas heater). Gas resistance returned as `uint32_t` ohms with `gas_valid` flag.

## Responsible human

@user
