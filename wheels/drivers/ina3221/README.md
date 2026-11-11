# ina3221 -- TI INA3221 triple-channel current and power monitor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the TI INA3221 triple-channel current/voltage
monitor. I2C interface injected via callbacks. Reads shunt voltage,
bus voltage, and calculates current for all three channels using the
configured shunt resistor values.

## Origin

Proactive build. INA3221 is widely used for multi-rail power monitoring
in battery-powered systems, server boards, and IoT gateways.

## Why self-written?

No existing open-source component for INA3221. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `ina3221_i2c_t` callbacks). No other dependencies.

## API

```c
ina3221_err_t ina3221_init(ina3221_t *dev, const ina3221_i2c_t *i2c,
                           const ina3221_cfg_t *cfg);
ina3221_err_t ina3221_read_channel(const ina3221_t *dev, uint8_t channel,
                                   float *shunt_uv, float *bus_mv);
ina3221_err_t ina3221_read_all(const ina3221_t *dev, ina3221_data_t *out);
```

Output: `ina3221_data_t { float shunt_uv[3]; float bus_mv[3]; float current_ma[3]; }`

## Usage

```c
ina3221_t mon;
ina3221_cfg_t cfg = {
    .addr = INA3221_ADDR_GND,
    .shunt_resistor_ohm = { 0.1f, 0.1f, 0.1f }
};
ina3221_init(&mon, &i2c_hal, &cfg);

ina3221_data_t data;
ina3221_read_all(&mon, &data);
// data.current_ma[0], data.bus_mv[1], etc.
```

## Configuration (compile-time)

None. I2C address and shunt resistor values are runtime-configurable
via `ina3221_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against TI SBOS576C. Not run on hardware.

- **Matched:** Addresses, both IDs, channel pointers, shunt and bus formats, byte order, the current formula, the POR Configuration the driver relies on.
- **Fixed:** The model now loads every POWER-ON RESET value of Table 8-1, keeps Shunt-Voltage Sum read-only and treats a write with RST as a power-on reset.
- **Still open:** The Configuration register cannot be reached, so averaging, conversion times, channel enable and operating mode stay at their reset values. There is no single-shot, power-down or software reset, no alert limits or Mask/Enable flags, and no power output.
- **Suspected:** A read within one sweep of power-up, up to 7.26 ms, returns the reset value 0000h as a valid 0 V or 0 A. The model's reads longer than 2 bytes step to the next register, which the manual never describes and the driver does not use.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/ina3221/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_ina3221
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
