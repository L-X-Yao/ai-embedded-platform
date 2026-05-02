# opt3001 — TI OPT3001 I2C digital ambient light sensor

**Status: candidate**
**Layer: L1 (drivers)**

Register-based I2C driver for the TI OPT3001 digital ambient light sensor.
Supports single-shot and continuous measurement modes with automatic full-scale
range. Result decoding uses the hardware exponent+mantissa format
(lux = 0.01 x 2^E x R). Pure C99, no heap, no OS dependency.
## Origin

Proactive build for ambient light sensing use cases (display auto-brightness,
environmental monitoring).


## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. Only OS-tied impls (Linux/RIOT). Self-written with HAL injection per platform conventions.

## Dependencies

- `i2c_hal` (injected via `opt3001_i2c_t` struct)

## API

```c
opt3001_err_t opt3001_init(opt3001_t *dev, const opt3001_i2c_t *i2c, uint8_t addr);
opt3001_err_t opt3001_read_lux(opt3001_t *dev, float *lux);
opt3001_err_t opt3001_start_single(opt3001_t *dev);
opt3001_err_t opt3001_set_continuous(opt3001_t *dev);
bool          opt3001_data_ready(opt3001_t *dev);
```

## Configuration (compile-time)

None. All configuration is done at runtime via API calls.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/opt3001/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_opt3001
```

## Test status

- Unit tests (sim): 25 tests, all passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: addr parameter in init (0x44-0x47), separate single/continuous mode setters, polling via data_ready helper

## Responsible human

@user
