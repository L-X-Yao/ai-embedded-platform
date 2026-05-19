# mcp4728 -- Microchip MCP4728 4-channel 12-bit DAC driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the Microchip MCP4728 4-channel 12-bit
digital-to-analog converter. I2C interface injected via callbacks.
Supports single-channel and all-channel writes using the fast write
command. 12-bit resolution (0-4095 values).

## Origin

Proactive build. MCP4728 is a popular multi-channel DAC for analog
signal generation in test equipment, audio, and control systems.

## Why self-written?

No existing open-source component for MCP4728. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `mcp4728_i2c_t` callbacks). No other dependencies.

## API

```c
mcp4728_err_t mcp4728_init(mcp4728_t *dev, const mcp4728_i2c_t *i2c,
                           const mcp4728_cfg_t *cfg);
mcp4728_err_t mcp4728_write_channel(mcp4728_t *dev, uint8_t channel,
                                    uint16_t value);
mcp4728_err_t mcp4728_write_all(mcp4728_t *dev, const uint16_t values[4]);
```

- `write_channel()` -- write one channel (0-3), values > 4095 rejected
- `write_all()` -- write all 4 channels in one I2C transaction

## Usage

```c
mcp4728_t dac;
mcp4728_cfg_t cfg = { .addr = MCP4728_ADDR_DEFAULT };
mcp4728_init(&dac, &i2c_hal, &cfg);

mcp4728_write_channel(&dac, 0, 2048);  /* channel 0 to mid-scale */

uint16_t vals[4] = { 0, 1024, 2048, 4095 };
mcp4728_write_all(&dac, vals);
```

## Configuration (compile-time)

None. I2C address is runtime-configurable via `mcp4728_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Known issues

- EEPROM write (persistent output on power-up) not implemented.
- Gain and voltage reference selection not implemented.

## How to test

```bash
cd wheels/drivers/mcp4728/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_mcp4728
```

## Test status

- Unit tests (sim): PASS
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-19
- **Key decisions**: Standard platform conventions. Context struct for multi-instance support where applicable.


## Responsible human

@user
