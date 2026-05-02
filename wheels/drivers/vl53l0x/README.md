# vl53l0x — ST VL53L0X I2C time-of-flight distance sensor

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the ST VL53L0X time-of-flight laser ranging
sensor. I2C interface injected via callbacks. Simplified init sequence
(not the full ST API) with single-shot ranging returning 16-bit millimeter
distance. Pure C99, no heap, fully reentrant per-context.
## Origin

Proactive build. VL53L0X is the most common I2C time-of-flight distance
sensor in embedded applications (robotics, drones, gesture detection,
proximity sensing). Range up to 2 m with mm resolution.

## Dependencies

I2C HAL (injected via `vl53l0x_i2c_t` callbacks). No other dependencies.

## API

```c
vl53l0x_err_t vl53l0x_init(vl53l0x_t *dev, const vl53l0x_i2c_t *i2c,
                            const vl53l0x_cfg_t *cfg);
vl53l0x_err_t vl53l0x_read_range_mm(vl53l0x_t *dev, uint16_t *range_mm);
vl53l0x_err_t vl53l0x_start_single(vl53l0x_t *dev);
vl53l0x_err_t vl53l0x_is_ready(vl53l0x_t *dev, bool *ready);
vl53l0x_err_t vl53l0x_set_measurement_timing_budget(vl53l0x_t *dev,
                                                     uint32_t budget_us);
```

- `read_range_mm`: blocking single-shot (start + poll + read)
- `start_single` + `is_ready`: non-blocking split for async usage
- `set_measurement_timing_budget`: 20000-500000 us range

Default I2C address: 0x29. Model ID register 0xC0 = 0xEE.

## Configuration (compile-time)

None. I2C address is runtime-configurable via `vl53l0x_cfg_t`.

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Known issues

- Uses simplified init (not the full ST VL53L0X API with SPAD calibration,
  ref calibration, temperature compensation). Sufficient for typical ranging
  applications but may not achieve maximum accuracy.
- Only single-shot mode. Continuous mode not implemented.
- Out-of-range (>2m) returns `VL53L0X_ERR_RANGE` (sensor reports 8190).

## How to test

```bash
cd wheels/drivers/vl53l0x/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_vl53l0x
```

30 assertions across 16 test cases: init OK, wrong model ID rejected, I2C
fail on init, null safety (9 paths), range read 152mm, range read 2000mm,
out-of-range sentinel, read timeout, read I2C fail, start_single register
verify, is_ready true/false, timing budget set + register verify, budget
bounds (too low / too high), init register config verify.

## Test status

- Unit tests (sim): PASS (16 C tests, 30 assertions)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Context struct (`vl53l0x_t`) holds I2C HAL + config + timing budget — fully reentrant for multi-sensor setups; simplified init (no SPAD/ref/temp calibration) to keep ~150 lines; split API (start_single + is_ready) alongside blocking read_range_mm for flexibility; 8190 sentinel detection returns ERR_RANGE; timing budget validated to 20-500 ms range.

## Responsible human
@user
