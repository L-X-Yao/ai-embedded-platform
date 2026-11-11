# vl53l0x — ST VL53L0X I2C time-of-flight distance sensor

**Status: candidate**
**Layer: L1 (drivers)**

> **Not usable on hardware yet.** `vl53l0x_init` skips the initialisation and calibration the manual requires before the first ranging.

Platform-agnostic driver for the ST VL53L0X time-of-flight laser ranging
sensor. I2C interface injected via callbacks. Simplified init sequence
(not the full ST API) with single-shot ranging returning 16-bit millimeter
distance. Pure C99, no heap, fully reentrant per-context.
## Origin

Proactive build. VL53L0X is the most common I2C time-of-flight distance
sensor in embedded applications (robotics, drones, gesture detection,
proximity sensing). Range up to 2 m with mm resolution.


## Why self-written?

Searched GitHub (2026-05-02). Referenced ST STSW-IMG005 API (proprietary) for register sequences and compensation algorithms. Complex init sequence; must reference official calibration tables. Self-written for platform convention consistency; cross-validated against reference.

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

## Datasheet check

Checked on 2026-11-06 against ST DocID029104 Rev 1, May 2016. That is the datasheet only: the API User Manual DocID029105 was not available, and this manual has no register map. Not run on hardware.

- **Matched:** Address 0x29, model ID 0xEE at 0xC0, MSB-first reads, the 20 ms minimum timing budget, the 30 ms default against Table 13.
- **Fixed:** The final range timeout went out as budget/2, so 30 ms wrote 0x3A98 and budgets from about 131 ms on all wrote 0xFFFF. The ST API holds it in macro periods as (LSByte * 2^MSByte) + 1. The driver now reads the VCSEL period at 0x70 and encodes it that way, so 30 ms gives 0x02C4 at 10 PCLKs.
- **Still open:** `vl53l0x_init` writes four registers and starts ranging with no device initialisation, no reference SPAD or temperature calibration and no calibration data, while the manual puts that phase before the first ranging. Porting the ST API is a large job, so ranging on hardware is unproven until then. Registers 0x00, 0x01, 0x0B, 0x14, 0x1E and 0x60, the ready bit and the 8190 sentinel could not be checked. The budget stops at 500 ms where the manual allows 5 s. No offset or cross-talk calibration, continuous ranging, GPIO1 interrupt or XSHUT control. The model sets the ready bit at once, and its tests pin 0xFF and 0x12 copied from the driver. The whole budget goes to the final range step, where the ST API first takes off the earlier steps, so a measurement takes a little longer than the budget.
- **Suspected:** The manual's range error code is never read, so a weak-signal result may pass as valid. The 500-try poll limit does not scale with the budget. `vl53l0x_init` does not wait the 1.2 ms boot time. There is no 2V8 mode setting.

## Known issues

- Uses simplified init (not the full ST VL53L0X API with SPAD calibration,
  ref calibration, temperature compensation). See the datasheet check above.
- Only single-shot mode. Continuous mode not implemented.
- Out-of-range (>2m) returns `VL53L0X_ERR_RANGE` (sensor reports 8190).

## How to test

```bash
cd wheels/drivers/vl53l0x/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_vl53l0x
```

19 test cases: init OK, wrong model ID rejected, I2C
fail on init, null safety (9 paths), range read 152mm, range read 2000mm,
out-of-range sentinel, read timeout, read I2C fail, start_single register
verify, is_ready true/false, timing budget set + register verify, budget
bounds (too low / too high), init register config verify.

## Test status

- Unit tests (sim): PASS (19 C tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Context struct (`vl53l0x_t`) holds I2C HAL + config + timing budget — fully reentrant for multi-sensor setups; simplified init (no SPAD/ref/temp calibration) to keep ~150 lines; split API (start_single + is_ready) alongside blocking read_range_mm for flexibility; 8190 sentinel detection returns ERR_RANGE; timing budget validated to 20-500 ms range.

## Responsible human
@L-X-Yao
