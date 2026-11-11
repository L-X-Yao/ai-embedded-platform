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

The register reads 0 until the first conversion ends, up to 15 ms after power-up, and in continuous mode no flag tells that from a real 0 C. So the first read after init returns `TMP102_ERR_NOT_READY` if it finds 0. Wait 15 ms and read again: that read is believed, so a sensor really at 0 C costs one retry, and a retry with no wait can still return 0 C.

## Configuration (compile-time)

No compile-time configuration. All settings via runtime API.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against TI SBOS397I (June 2024). Not run on hardware.

- **Matched:** Addresses, pointer values, the SD bit and its read-modify-write, the 12-bit temperature format and the Table 6-2 values, the model's power-up values, no chip ID.
- **Fixed:** An alert limit of 128 C or more wrapped to the other sign: 128 C became -128 C and 150 C became -106 C, which turns ALERT on at once. Limits now saturate at 127.9375 C and -128 C, and NaN returns `TMP102_ERR_PARAM`. In the model R1, R0 and AL are read-only, and results come from conversions, which stop with SD set. A read straight after `tmp102_init` returned 0 C with `TMP102_OK`, and the first read now returns `TMP102_ERR_NOT_READY` when it finds 0.
- **Still open:** There is no extended mode, so nothing above 128 C can be read or set, and no one-shot, conversion rate, thermostat mode, ALERT polarity, fault queue, AL status, limit read-back, SMBus alert response or general-call reset.
- **Suspected:** A read after `tmp102_wake` returns the old result with `TMP102_OK` until the next conversion ends. `tmp102_init` leaves EM as it finds it, so extended mode left on by earlier firmware halves the temperature.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/tmp102/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tmp102
```

## Test status

- Unit tests (sim): 66 tests (24 main + 31 hw_model + 9 null + 2 prop), all passing
- HIL: N/A

## Migration

From the fixes of 2026-11.

| Change | What a caller has to do | If they do nothing |
|--------|-------------------------|--------------------|
| The first read after init returns the new `TMP102_ERR_NOT_READY` if the register reads 0, and leaves `temp_c` alone | Wait 15 ms after init, or retry on this code after 15 ms. A chip really at 0 C needs that one retry | A caller that treats any error as fatal stops there. One that ignores the return keeps its old `temp_c` |
| `tmp102_t` grew by one `bool` | Nothing: treat it as opaque storage | Nothing changes for the caller |

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: 12-bit mode only (no extended 13-bit mode); alert
  thresholds set individually; float API for temperature (0.0625 C
  resolution makes integer API awkward).

## Responsible human

@L-X-Yao
