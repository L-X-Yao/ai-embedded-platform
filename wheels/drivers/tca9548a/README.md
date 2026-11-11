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

I2C HAL (injected via callbacks, raw pattern: the chip has no register numbers).

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

HAL: raw I2C. `write_raw` is required, and `tca9548a_get_channel` also needs
`read_raw`. `write_reg` and `read_regs` stay in the struct but are unused, so a
HAL that sets only those fails `tca9548a_init` with `TCA9548A_ERR_PARAM`.

```c
#include "tca9548a.h"

extern bool bus_write(void *ctx, uint8_t dev_addr, const uint8_t *data, uint16_t len);
extern bool bus_read(void *ctx, uint8_t dev_addr, uint8_t *data, uint16_t len);

static const tca9548a_i2c_t i2c = {
    .write_raw = bus_write,   /* exact bytes after the address, then STOP */
    .read_raw  = bus_read,    /* no register byte first */
};
```

Caches current channel mask to skip redundant I2C writes.

## Configuration (compile-time)

None.

## Supported platforms

- [x] Linux host (sim) -- passed 2026-05-01

## Datasheet check

Checked on 2026-11-06 against TI SCPS207H, May 2012, revised September 2024. Not run on hardware.

- **Matched:** Addresses 0x70-0x77, the control byte, several channels at once, the 0x00 default, no delay needed.
- **Fixed:** `tca9548a_get_channel` went through `read_regs`, and the chip stored the register byte as the control value: every channel switched off and the call returned `TCA9548A_OK` with mask 0. It now reads one byte through `read_raw` with nothing sent first, as in Figure 7-5. The channel writes sent 0x00 and then the mask, which worked only because the chip keeps the last byte. They now send the mask alone through `write_raw`, as in Figure 7-4. The mock and the model treated reg as a register number, which hid this. The model is now one control byte at bus level, with a NACK for a wrong address. It now connects channels at the STOP and has the RESET pin. None of this has run on hardware.
- **Still open:** There is no RESET pin control, the manual's way out of a stuck downstream bus.
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

- Unit tests: **31 C cases** -- init (ok, null dev, null i2c, addr low, addr high,
  all valid addrs, i2c fail, null write_raw, register HAL), select (bytes on the bus,
  ch0, ch7, invalid, null, cached skip, i2c fail), select_multi (ok, all, cached skip,
  null), disable_all (ok, already disabled, null), get_channel (ok, writes nothing,
  null read_raw, resync cache, i2c fail, null dev, null mask), workflow
- Model tests: **19 C cases** -- the same calls against the bus model, with the
  exact bytes of every write and the channels left on by a read

## API convention deviations

None.

## API review

- Date: 2026-05-01
- Key decisions: raw HAL (`write_raw`, `read_raw`, added 2026-11-06 in place of the register HAL with a reg=0x00 dummy); channel mask caching

## Responsible human

@L-X-Yao
