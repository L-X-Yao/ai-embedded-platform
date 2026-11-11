# bmm150 -- Bosch BMM150 3-axis geomagnetic sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

> **Readings are not calibrated.** The driver scales raw counts by nominal factors. The chip's trim data and temperature compensation are not applied, so the uT values are approximate and not fit for heading.

Platform-agnostic driver for the Bosch BMM150 3-axis magnetometer.
I2C interface injected via callbacks. Outputs magnetic field in
microtesla (uT) for X, Y, and Z axes. Uses linear scaling without
trim compensation.

## Origin

Proactive build. BMM150 is commonly paired with BMI270 or BMI160 for
9-axis IMU setups in drones, compasses, and navigation systems.

## Why self-written?

No existing open-source component for BMM150. Self-written for zero-dependency I2C HAL
injection and platform conventions.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `bmm150_i2c_t` callbacks). No other dependencies.

## API

```c
bmm150_err_t bmm150_init(bmm150_t *dev, const bmm150_i2c_t *i2c,
                          const bmm150_cfg_t *cfg);
bmm150_err_t bmm150_read(bmm150_t *dev, bmm150_data_t *out);
bmm150_err_t bmm150_reset(bmm150_t *dev);
```

Output: `bmm150_data_t { float x_ut; float y_ut; float z_ut; }`

## Usage

```c
bmm150_t mag;
bmm150_cfg_t cfg = { .addr = BMM150_ADDR_DEFAULT };
bmm150_init(&mag, &i2c_hal, &cfg);

bmm150_data_t data;
bmm150_read(&mag, &data);
// data.x_ut, data.y_ut, data.z_ut in microtesla
```

## Configuration (compile-time)

None. I2C address is runtime-configurable via `bmm150_cfg_t`.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-05 against BST-BMM150-DS001-05 and Bosch `bmm150.c`. Not run on hardware.

- **Matched:** Register addresses, I2C address, chip ID, power bit, operating mode, axis decoding, start-up delay.
- **Fixed:** `bmm150_reset` wrote 0x82 and so cleared the power bit, which suspends the chip; reset now keeps that bit and restarts measuring; overflow markers return `BMM150_ERR_OVERFLOW` instead of a huge reading. `delay_ms` is now required, because the chip needs up to 3 ms after power-on.
- **Still open:** Trim register read and Bosch compensation, which also settle the two scale factors. No RHALL or data ready check.

## Known issues

- Uses linear scaling without Bosch trim register compensation. Adequate
  for heading but not for precision magnetometry.

## How to test

```bash
cd wheels/drivers/bmm150/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bmm150
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
