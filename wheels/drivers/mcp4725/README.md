# mcp4725 — MCP4725 12-bit I2C DAC

**Status: candidate**
**Layer: L1 (drivers)**

Drives the Microchip MCP4725 single-channel 12-bit DAC over I2C.
Supports fast-mode write (volatile), DAC register write with power-down
control, and EEPROM write for persistent output across power cycles.

## Origin

Proactive build for analog output (ecosystem-driven, ADR-011). Fills the
DAC category gap — no existing DAC wheel in the platform.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-05):

- Adafruit_MCP4725: C++/Arduino, heap allocation, not suitable.
- Various Arduino libs: all tied to Wire.h, not portable C99.
- No suitable zero-dependency C99 implementation found.

Reference: Microchip DS22039D.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
mcp4725_err_t mcp4725_init(mcp4725_t *dev, const mcp4725_i2c_t *i2c, uint8_t addr);
mcp4725_err_t mcp4725_write_fast(mcp4725_t *dev, uint16_t value);
mcp4725_err_t mcp4725_write_dac(mcp4725_t *dev, uint16_t value, mcp4725_pd_t pd);
mcp4725_err_t mcp4725_write_eeprom(mcp4725_t *dev, uint16_t value, mcp4725_pd_t pd);
mcp4725_err_t mcp4725_read_status(mcp4725_t *dev, mcp4725_status_t *out);
```

## Configuration (compile-time)

None. Address passed at runtime via `mcp4725_init()`.

## Supported platforms

- [x] Host (sim) — 2026-05-05
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against Microchip DS22039D. Not run on hardware.

- **Matched:** Addresses, the Fast mode frame, command values, write data bytes, power-down codes, the 5-byte read length, RDY/BSY polarity, the EEPROM readback layout.
- **Fixed:** `mcp4725_write_dac` and `mcp4725_write_eeprom` put PD at bits 3:2 instead of 2:1, so `MCP4725_PD_1K` and `MCP4725_PD_500K` gave 100 kOhm, `MCP4725_PD_100K` left the output on, and the EEPROM stored that mode. `mcp4725_read_status` used the wrong read layout, so `dac_value` came back about a quarter of the true code and `pd_mode` was wrong in every power-down mode. The model and both mocks copied these layouts.
- **Still open:** The POR bit is not exposed. There is no General Call reset or wake-up and no voltage helper. `mcp4725_write_fast` always sends PD = 00, so it also wakes a powered-down DAC. `mcp4725_init` does not range-check the address. In the model an EEPROM write never goes busy and writes land while busy, so a write the device would ignore during the 25-50 ms EEPROM cycle cannot show up in tests. The model starts at 0, while a factory part starts at mid-scale, and it never loads the EEPROM into the DAC.
- **Suspected:** `mcp4725_write_eeprom` returns at once, so a write in the next 25-50 ms is ignored while the driver reports success. `mcp4725_init` skips the General Call Reset the manual recommends, which matters on a VDD ramp slower than 1 V/ms.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/mcp4725/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mcp4725
```

## Test status

- Unit tests (sim): 28/28 pass
- HIL: N/A

## API convention deviations

Uses raw write/read HAL instead of register-based write_reg/read_regs.
MCP4725 is command-based, not register-addressed.

## API review

- **Date**: 2026-05-05
- **Key decisions**: Raw I2C HAL (write/read) instead of register HAL
  because MCP4725 uses command bytes, not register addresses.

## Responsible human

@user
