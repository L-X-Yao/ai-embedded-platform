# ina219

**STATUS: candidate**
**Layer: L1 (drivers)**

TI INA219 I2C current/voltage/power monitor driver. Supports configurable
bus voltage range (16V/32V), PGA gain (40-320mV shunt voltage), and
automatic calibration from shunt resistor value. All registers are 16-bit
big-endian.
## API

```c
#include "ina219.h"

ina219_err_t ina219_init(ina219_t *dev, const ina219_i2c_t *i2c,
                         uint8_t dev_addr, float shunt_ohms,
                         ina219_bus_range_t range, ina219_gain_t gain);
ina219_err_t ina219_read_bus_voltage(const ina219_t *dev, float *voltage_v);
ina219_err_t ina219_read_shunt_voltage(const ina219_t *dev, float *voltage_mv);
ina219_err_t ina219_read_current(const ina219_t *dev, float *current_ma);
ina219_err_t ina219_read_power(const ina219_t *dev, float *power_mw);
ina219_err_t ina219_reset(ina219_t *dev);
```

## How to test

```bash
cd wheels/drivers/ina219/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Unit tests: **21 C cases** — init valid/null/bad params, bus voltage conversion,
  shunt voltage (positive and negative), current via calibration, power readout,
  config register bit verification (range/gain), calibration register calculation,
  I2C failure paths, reset, null output pointers, different shunt values

## Dependencies

I2C HAL (injected via callbacks). Requires `write_regs` for 16-bit register writes.

## Origin

Proactive build for current/power monitoring (2026-05-01).

## Known issues

None.

## Supported platforms

- [x] Linux host (sim) — passed 2026-05-01

## API review

- Date: 2026-05-01
- Key decisions: Calibration computed from shunt_ohms + gain; current_lsb
  derived automatically; all reads return float in standard units (V, mV, mA, mW)

## API convention deviations

None.

## Responsible human

@user
