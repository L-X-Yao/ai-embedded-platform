# bme688 — Bosch BME688 environmental sensor driver (temp/hum/pres/gas)

**Status: candidate**
**Layer: L1 (drivers)**

Bosch BME688 temperature + pressure + humidity + gas resistance (VOC) sensor
driver. I2C interface injected via callbacks. Implements Bosch compensation
algorithms in integer arithmetic.
Same chip ID as the BME680 (0x61), but run_gas, gas_r and the gas formula differ.

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

## Datasheet check

Checked on 2026-11-06 against BST-BME688-DS000-03 Revision_1.3 022024. Not run on hardware.

- **Matched:** Addresses, chip ID, soft reset, control and status registers, every calibration address, raw T/P/H packing, temperature and pressure compensation, gas_wait encoding, nb_conv.
- **Fixed:** run_gas was written to bit 4 instead of bit 5, so gas conversions never started. Gas data came from 0x2A and 0x2B instead of 0x2C and 0x2D. Gas resistance used the BME680 formula: 504014 ohm where the manual gives 4035400. Humidity read 4.096 times too high. The heater code did not follow the manual: 10 where it gives 120 at 300 degC, so the plate stayed cold. `gas_valid` ignored heat_stab_r. A heater over about 1 s always ended in a timeout. The model and mock repeated the run_gas and gas register errors.
- **Still open:** Once set, run_gas stays on and heat_off is never used, so every read heats the plate until a soft reset. A skipped channel's marker value comes back as real data with `BME688_OK`, and skipping temperature also spoils pressure and humidity. With a NULL `delay_ms` the polls run back to back, so a read with the heater on ends in `BME688_ERR_TIMEOUT`. The manual does not give the unit of amb_temp in the integer heater code: the driver uses 0.01 degC, which agrees with the floating point form, while `bme68x.c` divides by 1000 and loses the term. `range_sw_err` stays in `bme688_t` and 0x04 is still read, though the BME688 formula does not use it. Parallel mode, heater profiles, fields 1 and 2, idac_heat_x and SPI are not implemented.
- **Suspected:** `bme688_reset` does not wait and does not restore settings, so the next read returns skipped-channel markers as data. `bme688_init` starts without a soft reset, so its writes can be ignored after an MCU reset during a measurement. variant_id at 0xF0 is not checked, so a BME680 passes init and then gets the wrong gas handling. The reference above cites DS000-18, where this manual is DS000-03.

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

- Unit tests (sim): 46 pass, 0 fail
- HW model tests (sim): 16 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: BME688 shares the BME680's chip ID (0x61) but not all of its register map.
  API adds `bme688_cfg_t` for init-time configuration (oversampling, filter,
  gas heater). Gas resistance returned as `uint32_t` ohms with `gas_valid` flag.

## Responsible human

@user
