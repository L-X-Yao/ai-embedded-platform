# pac1952 — Microchip PAC1952 dual-channel I2C power/energy monitor

**Status: candidate**
**Layer: L1 (drivers)**

Dual-channel bus voltage, sense voltage, current, and power measurement.
VBUS: 16-bit unsigned, FSR = 32V. VSENSE: 16-bit signed, FSR = +/-100mV.
Configurable sense resistor per channel for current/power calculation.

## Origin

Proactive build for power monitoring use cases (battery systems, load profiling).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- No suitable zero-dependency C99 PAC1952 implementation found.
- Existing libraries are Arduino-class or C++ with heap allocation.

Reference: Microchip PAC1952 datasheet (DS20006539).

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
pac1952_err_t pac1952_init(pac1952_t *dev, const pac1952_i2c_t *i2c,
                           const pac1952_cfg_t *cfg);
pac1952_err_t pac1952_refresh(pac1952_t *dev);
pac1952_err_t pac1952_read_channel(pac1952_t *dev, uint8_t ch,
                                   pac1952_channel_t *out);
pac1952_err_t pac1952_read_both(pac1952_t *dev,
                                pac1952_channel_t *ch1,
                                pac1952_channel_t *ch2);
pac1952_err_t pac1952_set_sample_rate(pac1952_t *dev, pac1952_rate_t rate);
pac1952_err_t pac1952_get_product_id(pac1952_t *dev, uint8_t *id);
```

## Configuration (compile-time)

| Define | Default | Purpose |
|--------|---------|---------|
| (none) | — | All configuration via `pac1952_cfg_t` at runtime |

## Supported platforms

- [x] Host (sim) — 2026-05-06
- [ ] Real hardware target — not yet verified

## Known issues

- Mock I2C uses flat byte array, so adjacent multi-byte registers (e.g. VBUS1 at 0x07, VBUS2 at 0x08) share a byte in tests. Tests account for this with appropriate tolerance.

## How to test

```bash
cd wheels/drivers/pac1952/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_pac1952
```

## Test status

- Unit tests (sim): 40 pass (+ 12 generated null tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: write_reg/read_regs HAL (standard I2C pattern); per-channel sense resistor config; explicit refresh() before read for deterministic timing.

## Responsible human

@user
