# tmp102 — TI TMP102 I2C digital temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

12-bit digital temperature sensor (0.0625 C/LSB) with I2C interface.
Supports four selectable addresses (0x48-0x4B), programmable alert
thresholds, and shutdown/wake for low-power operation.
## Origin

Proactive build for common I2C temperature sensing use case.

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

## Known issues

None.

## How to test

```bash
cd wheels/drivers/tmp102/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tmp102
```

## Test status

- Unit tests (sim): 16 tests, all passing
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
