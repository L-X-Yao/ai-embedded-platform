# aw9523 — AW9523B I2C GPIO expander and LED driver (16 pins)

**Status: candidate**
**Layer: L1 (drivers)**

Awinic AW9523B: 16-pin I2C GPIO expander with constant-current LED driver.
Two 8-bit ports (P0/P1), each pin individually configurable as push-pull GPIO
or 256-step LED current sink (max ~37mA). Interrupt-on-change with per-pin mask.

## Origin

Proactive build for I2C GPIO/LED expander use cases (keyboard matrices, LED
arrays, general I/O expansion). Common in consumer electronics and IoT devices.

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-06):

- No suitable zero-dependency C99 implementation found.
- Most AW9523B code is Arduino/C++ or tightly coupled to specific frameworks.

Reference: Awinic AW9523B datasheet rev 1.5.

## Dependencies

None (I2C HAL injected via function pointers).

## API

```c
aw9523_err_t aw9523_init(aw9523_t *dev, const aw9523_i2c_t *i2c, uint8_t dev_addr);
aw9523_err_t aw9523_set_direction(aw9523_t *dev, aw9523_port_t port, uint8_t pin, aw9523_dir_t dir);
aw9523_err_t aw9523_write_pin(aw9523_t *dev, aw9523_port_t port, uint8_t pin, aw9523_level_t level);
aw9523_err_t aw9523_read_pin(aw9523_t *dev, aw9523_port_t port, uint8_t pin, aw9523_level_t *val);
aw9523_err_t aw9523_write_port(aw9523_t *dev, aw9523_port_t port, uint8_t val);
aw9523_err_t aw9523_read_port(aw9523_t *dev, aw9523_port_t port, uint8_t *val);
aw9523_err_t aw9523_set_led_mode(aw9523_t *dev, aw9523_port_t port, uint8_t pin, aw9523_led_mode_t mode);
aw9523_err_t aw9523_set_led_current(aw9523_t *dev, aw9523_port_t port, uint8_t pin, uint8_t current);
aw9523_err_t aw9523_set_interrupt_enable(aw9523_t *dev, aw9523_port_t port, uint8_t mask);
aw9523_err_t aw9523_reset(aw9523_t *dev);
```

## Configuration (compile-time)

No compile-time configuration needed.

## Supported platforms

- [x] Host (sim) — 2026-05-06
- [ ] Real hardware — not yet verified

## Datasheet check

Checked on 2026-11-06 against the AW9523B datasheet, May 2016, V1.1.1 (Shanghai Awinic). Not run on hardware.

- **Matched:** ID register, register map, Config, Int and LED mode bit meanings, the DIM map of all 16 pins, the 7FH reset write, CTL test bits held at 0, 256 steps and 37 mA IMAX.
- **Fixed:** `aw9523_init` refused 0x5A and 0x5B, the addresses with AD1 high. It wrote 0x00 to CTL and called that a reset, so DIM values survived an MCU restart; it now writes 00H to 7FH before any other write. It left P0 open-drain although the header promises push-pull. A comment and the model gave the wrong POR state for Config and Int. The model now resets LED mode to FFH, resets only on a write of 00H to 7FH, and has GPOMD and the Table 1 defaults, and `test_hw_model_aw9523.c` runs the driver on it.
- **Still open:** There is no ISEL or GPOMD control, so the LED range stays 0 to IMAX and P0 stays push-pull. RSTN is not handled: its internal pull-down holds the part in reset unless the board drives it high. This README and the source cite rev 1.5 and its Table 3, which were not available; in V1.1.1 the DIM map is in Table 4 and Table 13.
- **Suspected:** Interrupts stay enabled while init makes the pins inputs and Input is never read, so INTN may stay asserted after init, and the reset now opens that window on every init. An LED pin keeps Config = 1, and the manual does not say whether LED mode overrides it.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/aw9523/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_aw9523
```

## Test status

- Unit tests (sim): 60 tests passing
- Null tests: 13 auto-generated
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-06
- **Key decisions**: Per-pin direction/LED mode (read-modify-write), interrupt mask uses user-friendly 1=enabled convention with internal inversion, DIM register mapping per datasheet Table 3.

## Responsible human

@user
