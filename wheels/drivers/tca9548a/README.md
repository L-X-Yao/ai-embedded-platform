# tca9548a

**Status: candidate**
**Layer: L1 (drivers)**

TCA9548A 8-channel I2C multiplexer driver. Single control register where
each bit enables one downstream I2C bus (bit 0 = ch0, ..., bit 7 = ch7).
Used to resolve address conflicts when multiple identical I2C devices
share the same bus.

## Origin

Proactive build for I2C address conflict resolution (2026-05-01).

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. 1-register I2C device, trivially simple. Self-written with HAL injection per platform conventions.

## Dependencies

I2C HAL (injected via callbacks, register-based pattern).

## API

```c
#include "tca9548a.h"

tca9548a_err_t tca9548a_init(tca9548a_t *dev, const tca9548a_i2c_t *i2c,
                             uint8_t dev_addr);
tca9548a_err_t tca9548a_select(tca9548a_t *dev, uint8_t channel);
tca9548a_err_t tca9548a_select_multi(tca9548a_t *dev, uint8_t mask);
tca9548a_err_t tca9548a_disable_all(tca9548a_t *dev);
tca9548a_err_t tca9548a_get_channel(tca9548a_t *dev, uint8_t *mask);
```

HAL: register-based I2C (write_reg / read_regs with reg=0x00 dummy address).
Caches current channel mask to skip redundant I2C writes.

## Configuration (compile-time)

None.

## Supported platforms

- [x] Linux host (sim) -- passed 2026-05-01

## Datasheet check

Checked on 2026-11-06 against TI SCPS207H, May 2012, revised September 2024. Not run on hardware.

- **Matched:** Addresses 0x70-0x77, the control byte, several channels at once, the 0x00 default, how writes work, no delay needed.
- **Fixed:** The comments said the chip takes the first data byte and that reg=0x00 is a harmless dummy. It keeps the last byte, so the dummy is safe on writes only.
- **Still open:** `tca9548a_get_channel` misbehaves on hardware. A normal register `read_regs` HAL, such as flybox's, sends the 0x00 register byte before reading, and the chip stores that byte as the control value: every channel switches off, the real mask is lost, and the call returns `TCA9548A_OK` with mask 0, which also goes into the cached mask. Do not call it on a live bus until the HAL has a raw read. The mock and the model both treat reg as a register number, so the tests pass and hide this. There is no RESET pin control, the manual's way out of a stuck downstream bus. The model does not turn channels on at STOP and has no NACK for a wrong address.
- **Suspected:** `tca9548a_select`, `tca9548a_select_multi` and `tca9548a_disable_all` skip the bus when the cached mask matches, so after a RESET pulse or supply dip they can report success with the channel off, until `tca9548a_init` runs again.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/tca9548a/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **26 C cases** -- init (ok, null dev, null i2c, addr low, addr high,
  all valid addrs, i2c fail), select (ch0, ch7, invalid, null, cached skip, i2c fail),
  select_multi (ok, all, cached skip, null), disable_all (ok, already disabled, null),
  get_channel (ok, resync cache, i2c fail, null dev, null mask), workflow

## API convention deviations

None.

## API review

- Date: 2026-05-01
- Key decisions: register-based HAL with reg=0x00 dummy; channel mask caching

## Responsible human

@user
