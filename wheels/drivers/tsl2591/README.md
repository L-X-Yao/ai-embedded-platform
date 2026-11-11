# tsl2591 — AMS TSL2591 high-dynamic-range ambient light sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

I2C driver for the AMS TSL2591 ambient light sensor with 600M:1 dynamic range.
Dual channel (visible + IR) with configurable gain (1x to 9876x) and integration
time (100ms to 600ms). Returns computed lux plus raw channel data.

## Origin

Proactive build for ambient light sensing applications (ADR-011 ecosystem-driven).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- Adafruit_TSL2591_Library: Arduino/C++ with heap allocation, not suitable.
- No suitable zero-dependency C99 implementation found.

Reference: AMS TSL2591 datasheet (AMS-TMD document).

## Dependencies

None. I2C HAL injected via function pointers.

## API

```c
tsl2591_err_t tsl2591_init(tsl2591_t *dev, const tsl2591_i2c_t *i2c,
                           tsl2591_gain_t gain, tsl2591_itime_t itime);
tsl2591_err_t tsl2591_read(tsl2591_t *dev, tsl2591_data_t *out);
tsl2591_err_t tsl2591_read_raw(tsl2591_t *dev, uint16_t *ch0, uint16_t *ch1);
tsl2591_err_t tsl2591_set_gain(tsl2591_t *dev, tsl2591_gain_t gain);
tsl2591_err_t tsl2591_set_integration_time(tsl2591_t *dev, tsl2591_itime_t itime);
tsl2591_err_t tsl2591_enable(tsl2591_t *dev);
tsl2591_err_t tsl2591_disable(tsl2591_t *dev);
tsl2591_err_t tsl2591_get_status(tsl2591_t *dev, uint8_t *status);
```

## Configuration (compile-time)

None. All configuration is runtime via init parameters and setter functions.

## Supported platforms

- [x] Host (sim) -- 2026-05-06
- [ ] Real hardware target -- not yet verified

## Datasheet check

Checked on 2026-11-06 against ams TSL2591 datasheet, Apr. 2013, ams163.5. Not run on hardware.

- **Matched:** Address, ID, command byte, register addresses, ENABLE and CONFIG fields, gain and integration time codes, the four-byte read order.
- **Fixed:** `tsl2591_read` and `tsl2591_read_raw` ignored AVALID, so until the first integration ended they returned 0 counts with `TSL2591_OK`. They now return `TSL2591_ERR_NOT_READY`. No test built the model. `test_hw_model_tsl2591.c` now does, and the model keeps SAI and SRESET and fills the data and AVALID only at the end of a cycle. `test_read_lux_max_gain_600ms` now works its value out with 600 ms.
- **Still open:** There is no saturation check, so a clipped channel still gives a lux value with `TSL2591_OK`. There is no wait helper, and a 600 ms setting can take about 648 ms. Thresholds, PERSIST, interrupts, SRESET and the PID read are not supported.
- **Suspected:** This manual has no lux formula or 408 coefficient, so the lux result and its unit cannot be checked. `tsl2591_set_gain` and `tsl2591_set_integration_time` change the scale at once, so the next read can apply the new factor to old counts, up to a 9876-fold error. `tsl2591_init` sets AEN before CONFIG, so the first cycle runs at the reset settings.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/tsl2591/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_tsl2591
```

## Test status

- Unit tests (sim): 34 pass + 13 null tests
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Command byte prefix (0xA0|reg) for all register access; standard TSL2591 lux formula using CPL; IR-exceeds-visible clamped to 0 lux.

## Responsible human

@L-X-Yao
