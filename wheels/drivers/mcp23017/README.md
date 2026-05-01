# mcp23017 — I2C 16-bit GPIO expander driver

**Status: candidate**
**Layer: L1 (drivers)**

MCP23017 I2C GPIO expander with two independent 8-bit ports (A/B).
Supports direction, pullup, pin/port read/write, and interrupt-on-change
configuration. Bank=0 register addressing. Pure C99, no heap, reentrant.

## Why not VSF native?

VSF has no MCP23017 driver. The chip is a Microchip I2C GPIO expander,
not a standard peripheral that VSF would abstract.

## Origin

Proactive build for GPIO expansion use cases (button matrices, LED banks,
relay boards) on I2C-constrained MCUs.

## Dependencies

- i2c_hal (injected via callbacks, no compile-time dependency)

## API

```c
mcp23017_err_t mcp23017_init(mcp23017_t *dev, const mcp23017_i2c_t *i2c,
                             uint8_t dev_addr);
mcp23017_err_t mcp23017_set_direction(mcp23017_t *dev, mcp23017_port_t port,
                                      uint8_t mask);
mcp23017_err_t mcp23017_set_pullup(mcp23017_t *dev, mcp23017_port_t port,
                                   uint8_t mask);
mcp23017_err_t mcp23017_write_port(mcp23017_t *dev, mcp23017_port_t port,
                                   uint8_t value);
mcp23017_err_t mcp23017_read_port(mcp23017_t *dev, mcp23017_port_t port,
                                  uint8_t *value);
mcp23017_err_t mcp23017_write_pin(mcp23017_t *dev, mcp23017_port_t port,
                                  uint8_t pin, bool level);
mcp23017_err_t mcp23017_read_pin(mcp23017_t *dev, mcp23017_port_t port,
                                 uint8_t pin, bool *level);
mcp23017_err_t mcp23017_set_interrupt(mcp23017_t *dev, mcp23017_port_t port,
                                      uint8_t mask);
mcp23017_err_t mcp23017_read_interrupt_flag(mcp23017_t *dev,
                                            mcp23017_port_t port,
                                            uint8_t *flags);
```

## Configuration (compile-time)

No compile-time configuration needed. Device address (0x20-0x27) is
set at runtime via `mcp23017_init()`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware — not yet verified

## Known issues

None.

## How to test

```bash
cd wheels/drivers/mcp23017/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mcp23017
```

## Test status

- Unit tests (sim): 22 pass, 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Two-port model (A/B enum), pin ops via read-modify-write
  on OLAT, Bank=0 addressing only, address range validation in init.

## Responsible human

@user
