# pac1952 — Microchip PAC1952 dual-channel I2C power/energy monitor

**Status: candidate**
**Layer: L1 (drivers)**

> **Not usable on hardware yet.** `pac1952_refresh` sends the wrong command and the HAL cannot issue the Send Byte the chip expects, so measurements do not update. `pac1952_set_sample_rate` returns `PAC1952_ERR_UNSUPPORTED`.

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

## Datasheet check

Checked on 2026-11-05 against Microchip DS20006539E. Not run on hardware.

- **Matched:** CTRL, ACC_COUNT, VBUS, VSENSE and ID register addresses, register widths, VBUS format and LSB, current and power formulas.
- **Fixed:** Product ID was 0x59, which matches no part; PAC1952-1 is 0x79 and PAC1952-2 is 0x7D. VSENSE was read as signed, but the default range is unipolar, so 25 mV read as 50 mV. `pac1952_set_sample_rate` wrote the GPIO_ALERT2 bits and ACC_COUNT; it now refuses. Removed the wrong VPOWER addresses.
- **Still open:** REFRESH is at 0x00, not 0x1F, and needs a Send Byte plus a 1 ms wait. `pac1952_refresh` sends REFRESH_V with a data byte, where the manual allows a Send Byte only. CTRL needs a Block Write and a REFRESH afterwards. The I2C address is fixed at 0x10, which is the ADDRSEL-to-GND case only.

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
