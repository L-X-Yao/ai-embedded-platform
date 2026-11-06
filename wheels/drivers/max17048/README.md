# max17048 -- MAX17048 1-cell LiPo fuel gauge driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the Maxim MAX17048/MAX17049 ModelGauge
fuel gauge IC. I2C interface injected via callbacks. Reports cell
voltage, state-of-charge (SOC%), and charge/discharge rate without
requiring a current-sense resistor. Supports low-SOC alert threshold,
quick-start recalibration, and sleep/wake power modes.

## Origin

Proactive build. MAX17048 is the go-to fuel gauge for single-cell LiPo
battery monitoring in wearables, IoT devices, and portable electronics.

## Why self-written?

No existing open-source component for MAX17048. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `max17048_i2c_t` callbacks). `read_regs` and `write_raw`
are required. `write_reg` is no longer used and may be NULL. No other
dependencies.

## API

```c
max17048_err_t max17048_init(max17048_t *dev, const max17048_i2c_t *i2c);
max17048_err_t max17048_read(max17048_t *dev, max17048_data_t *out);
max17048_err_t max17048_read_voltage(max17048_t *dev, float *voltage_v);
max17048_err_t max17048_read_soc(max17048_t *dev, float *soc_pct);
max17048_err_t max17048_quickstart(max17048_t *dev);
max17048_err_t max17048_set_alert_threshold(max17048_t *dev, uint8_t threshold_pct);
max17048_err_t max17048_clear_alert(max17048_t *dev);
max17048_err_t max17048_sleep(max17048_t *dev);
max17048_err_t max17048_wake(max17048_t *dev);
max17048_err_t max17048_version(max17048_t *dev, uint16_t *ver);
```

Output: `max17048_data_t { float voltage_v; float soc_pct; float charge_rate; }`

## Usage

```c
static const max17048_i2c_t i2c_hal = {
    .read_regs = my_read_regs,
    .write_raw = my_write_raw,   /* register, MSB, LSB in one transaction */
};

max17048_t gauge;
max17048_init(&gauge, &i2c_hal);

max17048_data_t data;
max17048_read(&gauge, &data);
// data.voltage_v, data.soc_pct (0-100%), data.charge_rate (%/hr)
```

## Configuration (compile-time)

None. Fixed I2C address 0x36.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against Maxim 19-6171 Rev 7. Not run on hardware.

- **Matched:** I2C address, register addresses, VCELL, SOC and CRATE scaling, 16-bit reads, the quick-start value, CONFIG bits.
- **Fixed:** `max17048_sleep` never set MODE.EnSleep. `max17048_clear_alert` left the cause in STATUS. `max17048_init` now clears the reset indicator. The headers now say the MAX17049 has two cells and `voltage_v` is per cell. The model resets STATUS to 0x01__ and needs EnSleep to sleep. Every write went out as two 8-bit writes, which the manual says have no effect, so quick-start, the alert threshold, clear alert, sleep, wake and the reset indicator clear all returned OK and changed nothing. Each write is now one transaction of register, MSB and LSB through the new `write_raw` callback, and `max17048_init` returns `MAX17048_ERR_PARAM` without it. The model and mock stored single-byte writes, which hid this, and now ignore a write without both bytes. None of these fixes has run on hardware.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/max17048/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_max17048
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
