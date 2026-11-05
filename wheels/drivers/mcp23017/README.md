# mcp23017 — I2C 16-bit GPIO expander driver

**Status: candidate**
**Layer: L1 (drivers)**

MCP23017 I2C GPIO expander with two independent 8-bit ports (A/B).
Supports direction, pullup, pin/port read/write, and interrupt-on-change
configuration. Bank=0 register addressing. Pure C99, no heap, reentrant.
## Origin

Proactive build for GPIO expansion use cases (button matrices, LED banks,
relay boards) on I2C-constrained MCUs.


## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. All C impls HAL-specific. Self-written with HAL injection per platform conventions.

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

## Datasheet check

Checked on 2026-11-06 against Microchip DS20001952C, (c) 2005-2016. Not run on hardware.

- **Matched:** Address range, all 22 BANK=0 register addresses, POR values, IODIR and GPPU polarity, what GPIO and OLAT reads and writes do, INTF reads.
- **Fixed:** `mcp23017_init` never cleared a pending interrupt, so after an MCU-only reset that port raised no further interrupts. It now reads GPIOA and GPIOB last, and it no longer writes the read-only INTF and INTCAP registers.
- **Still open:** Init assumes BANK=0: if other firmware left BANK=1, its writes land on the wrong registers. There is no INTCAP read, so the port value at the moment of an interrupt is not available. No DEFVAL, INTCON, IOCON, IPOL or RESET control, so only interrupt-on-pin-change is reachable. The model has no interrupt engine, ignores IPOL, masks OLAT with IODIR on a GPIO write, does not refresh GPIO on an OLAT write, and keeps 0Ah and 0Bh as two bytes, so it cannot catch faults in those areas. Unverified and not from this manual: later revisions are believed to restrict GPA7 and GPB7 to outputs, so check one before using pin 7 as an input.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/mcp23017/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mcp23017
```

## Test status

- Unit tests (sim): 32 pass (main) + 56 pass (hw_model), 0 fail
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Two-port model (A/B enum), pin ops via read-modify-write
  on OLAT, Bank=0 addressing only, address range validation in init.

## Responsible human

@user
