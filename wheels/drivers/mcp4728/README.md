# mcp4728 -- Microchip MCP4728 4-channel 12-bit DAC driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the Microchip MCP4728 4-channel 12-bit
digital-to-analog converter. I2C interface injected via callbacks.
Supports single-channel and all-channel writes using the Multi-Write
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
- `write_all()` -- write all 4 channels in one I2C transaction; each output
  changes after its own last byte, and the HAL must accept 11 data bytes

## Usage

```c
mcp4728_t dac;
mcp4728_cfg_t cfg = { .addr = MCP4728_ADDR_DEFAULT,
                      .vref_internal = 0x0F,   /* all channels on 2.048 V */
                      .gain_x2 = 0x01 };       /* channel A at gain x2 */
mcp4728_init(&dac, &i2c_hal, &cfg);

mcp4728_write_channel(&dac, 0, 2048);  /* channel 0 to mid-scale */

uint16_t vals[4] = { 0, 1024, 2048, 4095 };
mcp4728_write_all(&dac, vals);
```

## Configuration (compile-time)

None. I2C address, reference and gain are runtime-configurable via
`mcp4728_cfg_t`. In `vref_internal` and `gain_x2`, bit n is channel n, and 0
means VDD and gain x1. Init returns `MCP4728_ERR_PARAM` for gain x2 on a channel
using VDD, where the device ignores it, and for any bit above bit 3. With gain
x2 the output still cannot go above VDD.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against Microchip DS22187E. Not run on hardware.

- **Matched:** Address, channel coding, data layout, UDAC = 0 output update, the 12-bit range.
- **Fixed:** The code called the 0x40 command a fast write, but it is Multi-Write, so nothing said what its zero upper nibble does. The name and header now say so; the bytes sent are the same. `mcp4728_write_all` made four I2C transactions and now makes one. The model and mock refused VREF, PD and Gx bits and took commands other than Multi-Write. Every write set VDD and gain x1, so a factory channel left the internal 2.048 V reference at its first write and gain x2 could not be reached. `vref_internal` and `gain_x2` in `mcp4728_cfg_t` now set both for each channel. This fix has not run on hardware. The model now loads an EEPROM at power-up, keeps the input and output registers apart, and reads as in Figure 5-15.
- **Still open:** With both fields at 0 a write still sets VDD and gain x1, and every write sets normal mode, so a factory channel changes output scale at its first write unless `vref_internal` is set. There is no LDAC or General Call Software Update, so the four outputs of `mcp4728_write_all` change one after another, and a bus error part way leaves the earlier channels updated. There is no power-down control, EEPROM write, read-back or address programming. `mcp4728_init` sends nothing, so a missing device shows up only at the first write.
- **Suspected:** The driver and model cite DS20002278, which may not be this manual's document number.

## Known issues

- EEPROM write (persistent output on power-up) not implemented.

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

@L-X-Yao
