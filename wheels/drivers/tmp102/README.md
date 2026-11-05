# tmp102 — TI TMP102 I2C digital temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

12-bit digital temperature sensor (0.0625 C/LSB) with I2C interface.
Supports four selectable addresses (0x48-0x4B), programmable alert
thresholds, and shutdown/wake for low-power operation.
## Origin

Proactive build for common I2C temperature sensing use case.


## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. 2-register I2C sensor, trivially simple. Self-written with HAL injection per platform conventions.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
tmp102_err_t tmp102_init(tmp102_t *dev, const tmp102_i2c_t *i2c,
                         const tmp102_cfg_t *cfg);
tmp102_err_t tmp102_read_temp(tmp102_t *dev, float *temp_c);
tmp102_err_t tmp102_set_alert_high(tmp102_t *dev, float temp_c);
tmp102_err_t tmp102_set_alert_low(tmp102_t *dev, float temp_c);
tmp102_err_t tmp102_shutdown(tmp102_t *dev);
tmp102_err_t tmp102_wake(tmp102_t *dev);
```

## Configuration (compile-time)

No compile-time configuration. All settings via runtime API.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against TI SBOS397I (June 2024). Not run on hardware.

- **Matched:** Addresses, pointer values, the SD bit and its read-modify-write, the 12-bit temperature format and the Table 6-2 values, the model's power-up values, no chip ID.
- **Fixed:** An alert limit of 128 C or more wrapped to the other sign: 128 C became -128 C and 150 C became -106 C, which turns ALERT on at once. Limits now saturate at 127.9375 C and -128 C, and NaN returns `TMP102_ERR_PARAM`.
- **Still open:** The model lets every configuration bit be written, although R1, R0 and AL are read-only. It refreshes the temperature even with SD set and answers the first read with a real value, so it cannot show the stale reads listed below. There is no extended mode, so nothing above 128 C can be read or set, and no one-shot, conversion rate, thermostat mode, ALERT polarity, fault queue, AL status, limit read-back, SMBus alert response or general-call reset.
- **Suspected:** A read straight after `tmp102_init` returns 0 C, and one after `tmp102_wake` the old result, both with `TMP102_OK`. `tmp102_init` leaves EM as it finds it, so extended mode left on by earlier firmware halves the temperature.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/tmp102/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tmp102
```

## Test status

- Unit tests (sim): 22 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: 12-bit mode only (no extended 13-bit mode); alert
  thresholds set individually; float API for temperature (0.0625 C
  resolution makes integer API awkward).

## Responsible human

@user
