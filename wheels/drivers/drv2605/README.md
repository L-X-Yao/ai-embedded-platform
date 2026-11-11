# drv2605 -- TI DRV2605L I2C haptic motor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for TI DRV2605L haptic motor driver IC. Fixed I2C
address 0x5A. Supports ERM and LRA actuators, waveform sequencer with 8 slots
(internal effect library 1-123), and real-time playback mode.
## Origin

Proactive build. DRV2605L is the most common I2C haptic driver in embedded
products (wearables, game controllers, touchscreen feedback, IoT devices).


## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. All alternatives Rust or Arduino C++. Self-written with HAL injection per platform conventions.

## Dependencies

I2C HAL (injected via `drv2605_i2c_t` callbacks). No other dependencies.

## API

```c
drv2605_err_t drv2605_init(drv2605_t *dev, const drv2605_i2c_t *i2c);
drv2605_err_t drv2605_set_actuator(drv2605_t *dev, drv2605_actuator_t type);
drv2605_err_t drv2605_set_waveform(drv2605_t *dev, uint8_t slot, uint8_t effect_id);
drv2605_err_t drv2605_go(drv2605_t *dev);
drv2605_err_t drv2605_stop(drv2605_t *dev);
drv2605_err_t drv2605_set_realtime_value(drv2605_t *dev, uint8_t value);
```

## Configuration (compile-time)

None. Actuator type is runtime-configurable via `drv2605_set_actuator()`.

## Supported platforms

- [x] Host (sim) -- passed 2026-05-01
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against TI SLOS854D (DRV2605L) and SLOS825E (DRV2605). Not run on hardware.

- **Matched:** I2C address, device ID values, register addresses, MODE values, the actuator bit, the GO bit.
- **Fixed:** `drv2605_set_realtime_value` never entered RTP mode, so the motor never ran. `drv2605_stop` and `drv2605_go` now leave RTP mode. `drv2605_set_actuator` now selects a ROM library to match the actuator. The init comment claimed a reset and standby that never happened. The model takes all its reset values from Table 3, clears GO when playback ends, and plays only in ready internal trigger mode with something queued. init never wrote the library register, and the DRV2605 sheet gives its default as 0x00, the Empty library, on p.33 and as 1 on p.36. Step 6 of TI's Initialization Procedure says to write it, so init now selects the library that fits the actuator in FEEDBACK.
- **Suspected:** An LRA runs closed-loop with no calibration path. init does not wait 250 us after power-up.

## Known issues

- Only internal trigger and real-time modes are exercised. External trigger,
  PWM input, audio-to-vibe, diagnostics, and calibration modes are defined
  in the enum but not specifically configured.
- No open-loop / closed-loop selection API (uses chip default).

## How to test

```bash
cd wheels/drivers/drv2605/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_drv2605
```

13 tests: init OK, init DRV2605 (non-L) accepted, wrong device ID rejected,
init I2C fail, null safety (7 paths), set mode ERM, set mode LRA,
set mode I2C fail, waveform slot write + boundary, slot out-of-range rejected,
go/stop register verification, go I2C fail, real-time value write.

## Test status

- Unit tests (sim): PASS (13 C tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Minimal context struct (only stores I2C HAL pointer) since DRV2605L has a fixed address and no calibration data to cache; `set_actuator` controls actuator type (ERM/LRA) via feedback register bit 7 rather than the mode register; accepts both DRV2605 (devid=3) and DRV2605L (devid=7) variants; waveform slots map directly to registers 0x04-0x0B.

## Responsible human

@L-X-Yao
