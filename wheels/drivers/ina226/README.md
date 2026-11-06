# ina226 — TI INA226 precision I2C current/power monitor driver

**Status: candidate**
**Layer: L1 (drivers)**

TI INA226 high-side/low-side current/power monitor with I2C interface. Supports
configurable averaging, conversion time, and shunt resistance calibration. Verifies
manufacturer ID (0x5449) and die ID (0x2260) on init.

## Origin

Proactive build for power monitoring use cases (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-11):

- Existing INA226 libraries are Arduino/HAL-dependent.
- No suitable zero-dependency C99 implementation found.

Reference: TI INA226 datasheet (SBOS547C).

## Dependencies

None (I2C HAL injected via function pointers). Links with `-lm` for float math.

## API

```c
ina226_err_t ina226_init(ina226_t *dev, const ina226_i2c_t *i2c,
                         const ina226_cfg_t *cfg);
ina226_err_t ina226_read(ina226_t *dev, ina226_data_t *out);
ina226_err_t ina226_reset(ina226_t *dev);
```

## Supported platforms

- [x] Host (sim) — 2026-05-11
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against TI SBOS547C. Not run on hardware.

- **Matched:** Addresses, manufacturer ID, register map, Config fields, voltage LSBs, the calibration formula.
- **Fixed:** `ina226_init` refused die ID 0x2261, which the manual lists, and it clamped calibration at 16 bits although the register has 15. CAL, current and power were truncated, so the manual's example gave CAL 2559 and 9999 mA; they are now rounded. The model computes Current and Power, acts on RST and ignores writes to read-only registers.
- **Suspected:** The OVF flag is never read. `ina226_reset` leaves calibration at 0, so later reads show zero current with `INA226_OK`. Config bit 14 is written as 0.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ina226/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ina226 && ./build/test_hw_model_ina226 && ./build/test_null_ina226
```

## Test status

- Unit tests (sim): 29 passed (18 unit + 5 hw_model + 6 null)
- HIL: N/A

## API convention deviations

Uses `write_regs`/`read_regs` (multi-byte) instead of `write_reg`/`read_regs` because
all INA226 registers are 16-bit. Matches the INA219 wheel pattern.

## API Review

Pending.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
