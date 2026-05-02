# i2c_scan — I2C bus scanner and device identifier

**Status: candidate**
**Layer: L2 (protocols)**

Debug utility that scans all 128 7-bit I2C addresses to find connected devices.
Probe callback is user-supplied (hardware-agnostic). Includes a lookup table for
common sensor/peripheral addresses.
## Origin

Proactive build for embedded debug/bringup workflows.


## Why self-written?

Searched GitHub (2026-05-02). No suitable lightweight zero-dependency C99 implementation found. I2C scanning is inherently HAL-dependent; our function-pointer abstraction is the right approach. Self-written per platform conventions.

## Dependencies

None.

## API

```c
typedef bool (*i2c_scan_probe_fn)(void *ctx, uint8_t addr);
typedef void (*i2c_scan_found_fn)(void *ctx, uint8_t addr);

typedef struct {
    uint8_t addresses[I2C_SCAN_MAX_DEVICES];
    uint8_t count;
} i2c_scan_result_t;

/* Scan default range 0x08..0x77 */
i2c_scan_err_t i2c_scan(probe, probe_ctx, result);

/* Scan sub-range [start, end] inclusive */
i2c_scan_err_t i2c_scan_range(probe, probe_ctx, start, end, result);

/* Scan with per-device callback (no array limit) */
i2c_scan_err_t i2c_scan_callback(probe, probe_ctx, found, found_ctx);

/* Identify known address -> human-readable name or NULL */
const char *i2c_scan_identify(uint8_t addr);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| `I2C_SCAN_MAX_DEVICES` | 16 | Max devices storable in `i2c_scan_result_t` |

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified (probe callback is hardware-agnostic)

## Known issues

None.

## How to test

```bash
cd wheels/protocols/i2c_scan/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_i2c_scan
```

## Test status

- Unit tests (sim): 15 tests, all passing
- HIL: N/A (hardware-agnostic via callback)

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Probe via callback (no HAL dependency); result struct has fixed array with overflow-safe count; identify() uses const lookup table.

## Responsible human

@user
