# bme280

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for Bosch BME280 digital temperature, humidity, and
pressure sensor. I2C interface injected via callbacks. Implements the Bosch
compensation from datasheet BST-BME280-DS001-24: fixed-point code for temperature
and humidity, double precision for pressure.
Extends BMP280 with humidity measurement (registers 0xFD-0xFE).
## Origin

Proactive build. BME280 is the most widely used environmental sensor in
embedded applications (weather stations, HVAC, indoor air quality, drones).


## Why self-written?

Searched GitHub (2026-05-02). Referenced boschsensortec/BME280_SensorAPI (BSD-3) for register sequences and compensation algorithms. Official Bosch driver; compensation formulas from datasheet BST-BME280-DS002-15, verified. Self-written for platform convention consistency; cross-validated against reference.

## Dependencies

I2C HAL (injected via `bme280_i2c_t` callbacks). No other dependencies.

## API

```c
bme280_err_t bme280_init(bme280_t *dev, const bme280_i2c_t *i2c,
                         const bme280_cfg_t *cfg);
bme280_err_t bme280_read(bme280_t *dev, bme280_data_t *out);
bme280_err_t bme280_set_oversampling(bme280_t *dev,
                                     bme280_oversampling_t osrs_t,
                                     bme280_oversampling_t osrs_p,
                                     bme280_oversampling_t osrs_h);
bme280_err_t bme280_reset(bme280_t *dev);
```

Output: `bme280_data_t { float temperature_c; float pressure_pa; float humidity_pct; }`

## Configuration (compile-time)

None. Oversampling and I2C address are runtime-configurable via `bme280_cfg_t`.

Set `osrs_t`, `osrs_p` and `osrs_h`, because 0 is `BME280_OVERSAMPLING_SKIP`:

| Skipped | `bme280_read` |
|---|---|
| all three | not reached: init and `bme280_set_oversampling` return `BME280_ERR_PARAM` |
| `osrs_p` | `pressure_pa` is 0 |
| `osrs_h` | `humidity_pct` is 0 |
| `osrs_t` | `temperature_c` is the last temperature measured, and pressure and humidity are compensated with it |

With `osrs_t` SKIP, init takes one temperature measurement, and pressure and
humidity lose accuracy as the real temperature moves away from it. Run init again
to refresh it. If `bme280_set_oversampling` skips temperature before any was
measured, the next read measures one first.

## Supported platforms

- [x] Host (sim) — passed 2026-05-01
- [ ] ARM Cortex-M — not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against BST-BME280-DS001-24, document revision 1.24, February 2024 (its page footers still print BST-BME280-DS001-23). Not run on hardware.

- **Matched:** Addresses, chip ID, register addresses, the reset word, ctrl_hum and ctrl_meas fields and their write order, oversampling codes, the measuring bit, the calibration table including the split dig_H4 and dig_H5, the 8-byte burst, and temperature, humidity and pressure compensation.
- **Fixed:** `bme280_read` compensated the 0x80000 and 0x8000 placeholders of a skipped channel and returned them as measurements, and a skipped temperature also shifted pressure and humidity. A skipped pressure or humidity now reads 0, a skipped temperature reads the last one measured, and all three skipped is refused. The file header called the double-precision pressure code 32-bit integer. The model wrote real data for skipped channels and applied ctrl_hum at once. The status poll stopped at measuring 0, which is also what the bit reads during the NVM copy before a conversion, so a read could return the previous sample: it now also waits for sleep mode, with a limit from section 9.1, as 500 reads timed out at x8 and x16 on a 400 kHz bus. Init and `bme280_reset` now wait for start-up and for im_update, and the first of those waits falls to the caller with no `delay_ms`. The model measured nothing in mode 10, started the data registers at 0x00, finished a conversion inside the ctrl_meas write and kept any value written to 0xE0, and it now follows the manual on these. None of this has run on hardware.
- **Still open:** Pressure uses the double-precision code, which the manual recommends only with an FPU: about 5400 clocks against 1400 for the integer code on a Cortex-M3. The integer code overflows on out-of-range calibration, so it needs guards first. Normal mode, the IIR filter, t_sb and SPI are not implemented.

## Known issues

- Uses forced mode on every read (not continuous). For high-frequency sampling,
  consider switching to normal mode (not yet implemented).
- BME280 also supports SPI — only I2C is implemented.

## How to test

```bash
cd wheels/drivers/bme280/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bme280
```

## Test status

- Unit tests (sim): PASS (29 C tests, 26 hw_model tests)
- HIL: N/A

## API convention deviations

None.

## API review

- **Date**: 2026-05-01
- **Key decisions**: Context struct (`bme280_t`) holds calibration + I2C HAL + config — fully reentrant for multi-sensor setups; Bosch compensation, fixed-point for temperature and humidity and double precision for pressure, converted to float at output; forced mode per-read to simplify state management; chip ID check (0x60) on init to fail fast and distinguish from BMP280 (0x58); humidity calibration split across two register blocks (0xA1 + 0xE1..0xE7) per datasheet; `set_oversampling` allows runtime reconfiguration without re-init.

## Responsible human
@user
