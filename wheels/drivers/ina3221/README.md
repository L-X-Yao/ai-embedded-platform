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

## Why not VSF native?

No VSF component for INA3221. Self-written for zero-dependency I2C HAL
injection and platform conventions.

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

## Responsible human

@user
