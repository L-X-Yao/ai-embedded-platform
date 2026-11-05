# max17048 -- MAX17048 1-cell LiPo fuel gauge driver

**Status: candidate**
**Layer: L1 (drivers)**

> **Writes do nothing on hardware yet.** The HAL can only send 8-bit writes and the chip ignores them. Reads work. See Datasheet check.

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

I2C HAL (injected via `max17048_i2c_t` callbacks). No other dependencies.

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
- **Fixed:** `max17048_sleep` never set MODE.EnSleep. `max17048_clear_alert` left the cause in STATUS. `max17048_init` now clears the reset indicator. The headers now say the MAX17049 has two cells and `voltage_v` is per cell. The model resets STATUS to 0x01__ and needs EnSleep to sleep.
- **Still open:** Every write goes out as two 8-bit writes, which the manual says have no effect, so quick-start, the alert threshold, clear alert, sleep and wake all return OK and change nothing. The HAL needs a 16-bit write. The model and mock store single-byte writes, which hides this.

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
