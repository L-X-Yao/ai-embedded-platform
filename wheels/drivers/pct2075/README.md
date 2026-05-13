# pct2075 — NXP PCT2075 digital temperature sensor

**Status: candidate**
**Layer: L1 (drivers)**

11-bit I2C temperature sensor, LM75 successor with higher resolution (0.125 C vs 0.5 C).
Range -55 C to +125 C. Configurable address via A0-A2 pins (0x48-0x4F).

## Origin

Proactive build for temperature sensing use cases (ADR-011 ecosystem-driven).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- Existing LM75 drivers cover 9-bit only; PCT2075 needs 11-bit (shift-right-5) conversion.
- No suitable zero-dependency C99 implementation found that matches our wheel contract.

Reference: NXP PCT2075 datasheet.

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
pct2075_err_t pct2075_init(pct2075_t *dev, const pct2075_i2c_t *i2c,
                            const pct2075_cfg_t *cfg);
pct2075_err_t pct2075_read(pct2075_t *dev, float *temp_c);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `CONTRACT_ENABLED` | off | Enable Design-by-Contract checks |

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/pct2075/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_pct2075
./build/test_hw_model_pct2075
./build/test_null_pct2075
```

## Test status

- Unit tests (sim): 37 tests, all passing (22 mock + 10 hw_model + 5 null)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: 2-function API (init + read), cfg struct for address and optional config write, error enum OK=0/ERR_I2C=-1/ERR_PARAM=-2

## Responsible human

@user
