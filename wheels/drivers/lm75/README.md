# lm75 — LM75/LM75A/LM75B I2C digital temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for LM75/LM75A/LM75B digital temperature sensors.
I2C interface injected via callbacks. 9-bit resolution (0.5 C steps),
range -55 C to +125 C. Supports configurable OS and Thyst thresholds.

## Origin

Proactive build. LM75 is one of the most common I2C temperature sensors
in embedded systems (thermostats, server monitoring, industrial control).

## Why self-written?

Searched GitHub (2026-05-12). No suitable zero-dependency, zero-heap C99
implementation found that matches our wheel conventions (injected HAL,
error enum, no OS dependency). Extremely simple sensor (4 registers).

Reference: NXP LM75B datasheet (Rev. 8).

## Dependencies

I2C HAL (injected via `lm75_i2c_t` callbacks). No other dependencies.

## API

```c
lm75_err_t lm75_init(lm75_t *dev, const lm75_i2c_t *i2c);
lm75_err_t lm75_init_addr(lm75_t *dev, const lm75_i2c_t *i2c, uint8_t addr);
lm75_err_t lm75_read_temp(lm75_t *dev, float *temp_c);
lm75_err_t lm75_read_raw(lm75_t *dev, int16_t *raw);
lm75_err_t lm75_set_os_temp(lm75_t *dev, float temp_c);
lm75_err_t lm75_set_hyst_temp(lm75_t *dev, float temp_c);
```

## Configuration (compile-time)

None. I2C address is runtime-configurable via `lm75_init_addr()`.

## Supported platforms

- [x] Host (sim) — passed 2026-05-12
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against TI SNIS153D (LM75B, LM75C). The NXP LM75B sheet cited above could not be downloaded, so LM75A and NXP behaviour is unchecked. Not run on hardware.

- **Matched:** Addresses, the temperature read as a bus check, pointer values, register widths, byte order, scaling.
- **Fixed:** All 16 bits went into the temperature although D0 to D6 are undefined, which could add up to 0.496 C. NXP parts use D5 and D6 for finer data, which the driver drops to stay at its stated 9 bits.
- **Suspected:** Each access restarts the conversion, so polling faster than 300 ms freezes the reading.

## Known issues

- Only supports 9-bit resolution (LM75 default). LM75A/LM75B 11-bit mode
  not yet implemented.

## How to test

```bash
cd wheels/drivers/lm75/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_lm75
```

## Test status

- Unit tests (sim): PASS (17 C tests)
- HW model tests: PASS (9 tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-12
- **Key decisions**: Minimal context struct (i2c + addr); no chip ID check since LM75 lacks one (init verifies I2C comms by reading temp register); float output for ease of use; raw register access via read_raw for users who need it.

## Responsible human

@L-X-Yao
