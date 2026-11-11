# pac1952 — Microchip PAC1952 dual-channel I2C power/energy monitor

**Status: candidate**
**Layer: L1 (drivers)**

Dual-channel bus voltage, sense voltage, current, and power measurement.
VBUS: 16-bit unsigned, FSR = 32V. VSENSE: 16-bit unsigned, FSR = 100mV.
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

HAL: `read_regs` is required. `pac1952_refresh` and `pac1952_set_sample_rate` also
need `write_raw` and `delay_ms`, and without them return `PAC1952_ERR_PARAM` and
send nothing. `write_reg` stays in the struct but is unused.

`pac1952_cfg_t.addr` is the address the ADDRSEL resistor picks, 0x10 to 0x1F (Table 6-1). 0 means 0x10, and anything else outside the table returns `PAC1952_ERR_PARAM`.

```c
#include "pac1952.h"

extern bool bus_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                          uint8_t *buf, uint16_t len);
extern bool bus_write(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
extern void bus_delay_ms(void *ctx, uint32_t ms);

static const pac1952_i2c_t i2c = {
    .read_regs = bus_read_regs,
    .write_raw = bus_write,      /* exact bytes after the address, then STOP */
    .delay_ms  = bus_delay_ms,   /* waits at least ms */
};
```

Both calls block for 1 ms. Call `pac1952_init` at least 50 ms after VDD comes up.

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
- **Fixed:** Product ID was 0x59, which matches no part; PAC1952-1 is 0x79 and PAC1952-2 is 0x7D. VSENSE was read as signed, but the default range is unipolar, so 25 mV read as 50 mV. Removed the wrong VPOWER addresses. `pac1952_refresh` sent REFRESH_V (0x1F) with a data byte and did not wait. It now sends REFRESH at 0x00 as a Send Byte through `write_raw` and waits 1 ms through `delay_ms`, because the registers settle in that time and the chip NACKs commands sent sooner. `pac1952_set_sample_rate` wrote the GPIO_ALERT2 bits and ACC_COUNT, and then refused with `PAC1952_ERR_UNSUPPORTED`. It now reads CTRL, sets SAMPLE_MODE in bits 15-12, sends both bytes in one Block Write and then REFRESH. It uses the plain rate modes, and the new rate can start after the call returns, at the end of the conversion cycle. The model encoded the old mistakes and had no test; it is now a bus model from the manual. None of the 2026-11-06 fixes have run on hardware. The I2C address was fixed at 0x10, the ADDRSEL-to-GND case only. It now comes from `pac1952_cfg_t.addr`, and all 16 addresses of Table 6-1 work.
- **Still open:** High byte first in CTRL comes from the register drawing, as no sentence in the manual states it.

## Known issues

- Mock I2C uses flat byte array, so adjacent multi-byte registers (e.g. VBUS1 at 0x07, VBUS2 at 0x08) share a byte in tests. Tests account for this with appropriate tolerance.

## How to test

```bash
cd wheels/drivers/pac1952/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_pac1952
```

## Test status

- Unit tests (sim): 46 pass (+ 12 generated null tests)
- Model tests (sim): 26 pass
- HIL: N/A

## Migration

From the fixes of 2026-11.

| Change | What a caller has to do | If they do nothing |
|--------|-------------------------|--------------------|
| `pac1952_cfg_t` gained `addr`, where 0 means the old 0x10 | Set it if ADDRSEL is not tied to GND. Zero the struct or use a designated initializer | With a zeroed config, nothing changes. An uninitialized one can now return `PAC1952_ERR_PARAM`, or use a wrong address |
| `pac1952_t` grew with the config | Nothing: treat it as opaque storage | Nothing changes for the caller |

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: read_regs HAL, with write_raw and delay_ms added 2026-11-06 for Send Byte, Block Write and the REFRESH wait, in place of write_reg; per-channel sense resistor config; explicit refresh() before read for deterministic timing.

## Responsible human

@L-X-Yao
