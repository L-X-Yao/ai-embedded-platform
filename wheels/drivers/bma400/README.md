# bma400 — Bosch BMA400 ultra-low-power 3-axis accelerometer driver

**Status: candidate**
**Layer: L1 (drivers)**

Bosch BMA400 I2C driver with 12-bit signed acceleration output in milli-g,
configurable full-scale range (2/4/8/16g) and power mode (sleep/low-power/normal).

## Origin

Proactive build for ultra-low-power motion sensing use cases (ADR-011).

## Why self-written?

Searched GitHub/embedded ecosystem (2026-05-13):

- Bosch official `BMA400-API` is BSD-licensed but tightly coupled to their SensorAPI abstraction; too heavy for zero-dependency wheel pattern.
- No suitable zero-dependency C99 implementation found.

Reference: BST-BMA400-DS000-02 (Bosch BMA400 datasheet).

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
bma400_err_t bma400_init(bma400_t *dev, const bma400_i2c_t *i2c,
                         const bma400_cfg_t *cfg);
bma400_err_t bma400_read(bma400_t *dev, bma400_data_t *out);
bma400_err_t bma400_set_power(bma400_t *dev, bma400_power_t mode);
bma400_err_t bma400_set_range(bma400_t *dev, bma400_range_t range);
```

## Configuration (compile-time)

| Define             | Default | Purpose                          |
|--------------------|---------|----------------------------------|
| CONTRACT_ENABLED   | off     | Enable runtime contract checks   |

## Supported platforms

- [x] Host (sim) — 2026-05-13
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-05 against BST-BMA400-DS000-14 and Bosch `bma400.c`. Not run on hardware.

- **Matched:** Register addresses, chip ID, I2C addresses, range and power encodings, data layout, sensitivity.
- **Fixed:** Init and `bma400_set_range` overwrote ACC_CONFIG1 and left a reserved ODR code; range is now set before the power mode and read back, so a dropped write cannot scale readings wrongly; reading in sleep mode and after a failed init is refused; mode changes wait 10 ms when `delay_ms` is given.
- **Still open:** Data ready is not checked. The 10 ms wait comes from Bosch's driver, not the datasheet.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/bma400/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bma400
./build/test_hw_model_bma400
```

## Test status

- Unit tests (sim): 28 tests (mock) + 14 tests (hw_model)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-13
- **Key decisions**: 12-bit sign extension per datasheet; output in milli-g (not g) for integer-friendly downstream; sensitivity table matches datasheet LSB/g values.

## Responsible human

@user
