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

## Datasheet check

Checked on 2026-11-06 against TI SBOS681C. Not run on hardware.

- **Matched:** Addresses and the init range check, register addresses, both IDs, the result format and lux formula, mode and CRF fields, the default config value, byte order.
- **Still open:** The address macros are named after two pins, but the part has one ADDR pin: 0x46 needs it on SDA and 0x47 on SCL, so wiring by `OPT3001_ADDR_VDD_GND` or `OPT3001_ADDR_VDD_VDD` gives a device that does not answer. `opt3001_read_lux` ignores OVF and CRF and `OPT3001_ERR_NOT_READY` is never returned, so a read straight after init gives 0 lux with `OPT3001_OK`. `opt3001_data_ready` clears CRF by reading it, so a second call after a finished conversion returns false, and the config reads in `opt3001_start_single` and `opt3001_set_continuous` clear it too. There is no shutdown call, 800 ms mode, manual range, limit, INT or reset support, and no wait helper. The model resets Config to C010h instead of C810h, so the init test passes even if init writes nothing. It never clears CRF, finishes a conversion inside the write and leaves M at 01b, ignores M = 10b, lets OVF, FH and FL be written, and encodes lux wrongly (50000 lux reads back as 83865.6), so tests cannot show a conversion in progress.

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
