# tca9548a

**Status: candidate**
**Layer: L1 (drivers)**

TCA9548A 8-channel I2C multiplexer driver. Single control register where
each bit enables one downstream I2C bus (bit 0 = ch0, ..., bit 7 = ch7).
Used to resolve address conflicts when multiple identical I2C devices
share the same bus.
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

## Dependencies

I2C HAL (injected via callbacks, register-based pattern).

## Configuration (compile-time)

None.

## Origin

Proactive build for I2C address conflict resolution (2026-05-01).

## Known issues

None.

## Supported platforms

- [x] Linux host (sim) -- passed 2026-05-01

## API review

- Date: 2026-05-01
- Key decisions: register-based HAL with reg=0x00 dummy; channel mask caching

## API convention deviations

None.

## Responsible human

@user
