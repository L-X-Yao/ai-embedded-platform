# veml7700 — Vishay VEML7700 I2C ambient light sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

16-bit register-based I2C ambient light sensor with configurable gain (1/8x to 2x) and integration time (25ms to 800ms). Provides ALS and White channel raw reads plus computed lux output. Alternative to BH1750 with better dynamic range (0.0036 to 1.8432 lux/bit resolution).

## Origin

Proactive build for ambient light sensing with wider dynamic range than BH1750.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. All C impls platform-specific. Self-written with HAL injection per platform conventions.

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
/* Error codes: VEML7700_OK=0, VEML7700_ERR_I2C=-1, VEML7700_ERR_PARAM=-2 */
/* Gain: VEML7700_GAIN_1=0, VEML7700_GAIN_2=1, VEML7700_GAIN_1_8=2, VEML7700_GAIN_1_4=3 */
/* Integration time: VEML7700_IT_25MS=12, VEML7700_IT_50MS=8, VEML7700_IT_100MS=0,
 *                   VEML7700_IT_200MS=1, VEML7700_IT_400MS=2, VEML7700_IT_800MS=3 */

veml7700_err_t veml7700_init(veml7700_t *dev, const veml7700_i2c_t *i2c, veml7700_gain_t gain, veml7700_it_t it);
veml7700_err_t veml7700_read_als(veml7700_t *dev, uint16_t *raw);
veml7700_err_t veml7700_read_white(veml7700_t *dev, uint16_t *raw);
veml7700_err_t veml7700_read_lux(veml7700_t *dev, float *lux);
veml7700_err_t veml7700_set_gain(veml7700_t *dev, veml7700_gain_t gain);
veml7700_err_t veml7700_set_integration_time(veml7700_t *dev, veml7700_it_t it);
veml7700_err_t veml7700_power_on(veml7700_t *dev);
veml7700_err_t veml7700_shutdown(veml7700_t *dev);
```

## Configuration (compile-time)

None. All configuration is runtime via `veml7700_init()`, `veml7700_set_gain()`, and `veml7700_set_integration_time()`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/veml7700/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_veml7700
```

## Test status

- Unit tests (sim): 16 pass
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Fixed address 0x10 (no addr pin on VEML7700). 16-bit LE register protocol. Resolution table from datasheet. write_regs/read_regs HAL to handle 2-byte register values.

## Responsible human

@user
