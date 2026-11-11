# si7021 — I2C temperature and humidity sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Si7021 / HTU21D / SHT21 compatible driver. No-hold measurement mode with
CRC-8 validation, configurable resolution (14/12/13/11-bit temperature,
12/8/10/11-bit humidity). HAL-injected I2C with delay callback.

## Origin

Proactive build for common I2C environmental sensor use cases.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. All C impls HAL-specific. Self-written with HAL injection per platform conventions.

## Dependencies

None (I2C HAL injected via callbacks).

## API

```c
si7021_err_t si7021_init(si7021_t *dev, const si7021_i2c_t *i2c);
si7021_err_t si7021_read_humidity(si7021_t *dev, float *rh);
si7021_err_t si7021_read_temperature(si7021_t *dev, float *temp_c);
si7021_err_t si7021_read_both(si7021_t *dev, float *rh, float *temp_c);
si7021_err_t si7021_set_resolution(si7021_t *dev, si7021_resolution_t res);
si7021_err_t si7021_reset(si7021_t *dev);
si7021_err_t si7021_read_serial(si7021_t *dev, uint32_t *serial_a, uint32_t *serial_b);
uint8_t      si7021_crc8(const uint8_t *data, uint16_t len);
```

Fixed I2C address: `0x40`.

## Configuration (compile-time)

No compile-time configuration. Resolution is set at runtime via `si7021_set_resolution()`.

## Supported platforms

- [x] Host (sim) — 2026-05-01
- [ ] Real hardware target — not yet verified

## Datasheet check

Checked on 2026-11-06 against Silicon Labs Si7021-A20 Rev. 1.2 (8/16). Not run on hardware.

- **Matched:** Address, commands, the RH and temperature formulas, the 25 ms wait, the no-hold sequence, CRC-8 parameters, the user register and its resolution encoding, the serial number layout.
- **Fixed:** `si7021_reset` returned at once, although the sensor needs up to 15 ms. `si7021_init` accepted a NULL `delay_ms`, and every measurement then failed on the NACK during conversion, so init now returns `SI7021_ERR_PARAM`. The model had no conversion time and hid both, so it now keeps time through `delay_ms`. Its RH and temperature codes now end in the bits the manual fixes, a user register write changes only RES1, HTRE and RES0, and its default serial has 0x15 in SNB_3.
- **Still open:** `si7021_init` touches no hardware, so the power-up wait of up to 80 ms is the caller's. SNB_3 is never compared with 0x15, so a different part is not detected. There is no heater control, firmware revision or VDDS read, and no hold-master commands.
- **Suspected:** `si7021_read_serial` does not check its CRC bytes, so corrupt data returns `SI7021_OK`, but the manual does not say which bytes each one covers.

## Known issues

None.

## How to test

```bash
cd wheels/drivers/si7021/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_si7021
```

## Test status

- Unit tests (sim): 32 passing
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Command-based I2C (not register-based); no-hold mode with delay callback; read_both uses CMD_READ_TEMP_FROM_RH for efficiency.

## Responsible human

@L-X-Yao
