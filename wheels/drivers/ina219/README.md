# ina219

**Status: candidate**
**Layer: L1 (drivers)**

TI INA219 I2C current/voltage/power monitor driver. Supports configurable
bus voltage range (16V/32V), PGA gain (40-320mV shunt voltage), and
automatic calibration from shunt resistor value. All registers are 16-bit
big-endian.

## Origin

Proactive build for current/power monitoring (2026-05-01).

## Why self-written?

Searched GitHub (2026-05-02). Found libdriver/ina219 (MIT). Straightforward I2C. Self-written for unified HAL injection pattern, error codes, and test framework.

## Dependencies

I2C HAL (injected via callbacks). Requires `write_regs` for 16-bit register writes.

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

## Supported platforms

- [x] Linux host (sim) — passed 2026-05-01

## Datasheet check

Checked on 2026-11-06 against TI SBOS448G, as a recheck of the May review. Not run on hardware.

- **Matched:** Register map, gain and range fields, the calibration formula, current and power scaling.
- **Fixed:** Calibration could be odd, though bit 0 does not exist, which left the scale slightly off at the 80 mV gain. The 32 V range comment now gives the 26 V input limit.
- **Still open:** The model always returns zero in the low bits of Bus Voltage, so no test proves the flags are dropped.
- **Suspected:** `ina219_reset` leaves calibration at 0, so current and power then read 0. The manual does not say how the Power register behaves with reverse current.

## Known issues

None.

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

## API convention deviations

None.

## API review

- Date: 2026-05-01
- Key decisions: Calibration computed from shunt_ohms + gain; current_lsb
  derived automatically; all reads return float in standard units (V, mV, mA, mW)

## Responsible human

@user
