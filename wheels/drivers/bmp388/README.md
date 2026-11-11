# bmp388 -- Bosch BMP388 barometric pressure and temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the Bosch BMP388 barometric pressure and
temperature sensor. I2C interface injected via callbacks. Implements
the Bosch float compensation algorithm from datasheet BST-BMP388-DS001
(Sections 9.2 and 9.3) for temperature and pressure output.

## Origin

Proactive build. BMP388 is the successor to BMP280 with higher accuracy
and lower noise, used in drones, weather stations, and altimeters.

## Why self-written?

No existing open-source component for BMP388. Self-written with compensation formulas
cross-validated against the Bosch datasheet.

Searched GitHub (ADR-010 compliance). Self-written for platform convention consistency.


## Dependencies

I2C HAL (injected via `bmp388_i2c_t` callbacks). No other dependencies.

## API

```c
bmp388_err_t bmp388_init(bmp388_t *dev, const bmp388_i2c_t *i2c,
                         const bmp388_cfg_t *cfg);
bmp388_err_t bmp388_read(bmp388_t *dev, bmp388_data_t *out);
bmp388_err_t bmp388_reset(bmp388_t *dev);
```

Output: `bmp388_data_t { float temperature_c; float pressure_pa; }`

## Usage

```c
bmp388_t sensor;
bmp388_cfg_t cfg = { .addr = BMP388_ADDR_LOW,
                     .osrs_t = BMP388_OVERSAMPLING_2,
                     .osrs_p = BMP388_OVERSAMPLING_16 };
bmp388_init(&sensor, &i2c_hal, &cfg);

bmp388_data_t data;
bmp388_read(&sensor, &data);
// data.temperature_c, data.pressure_pa
```

## Configuration (compile-time)

None. Oversampling and I2C address are runtime-configurable via
`bmp388_cfg_t`.

Set both `osrs_t` and `osrs_p`, because 0 is `BMP388_OVERSAMPLING_SKIP`:

| `osrs_t` | `osrs_p` | `bmp388_read` |
|---|---|---|
| SKIP | SKIP | not reached: init returns `BMP388_ERR_PARAM` |
| set | SKIP | measures temperature only; `pressure_pa` is 0 |
| SKIP | set | measures pressure only; `temperature_c` is the temperature init measured, and the pressure is compensated with it |

With `osrs_t` SKIP, init takes one temperature measurement, and the pressure
loses accuracy as the real temperature moves away from it. Run init again to
refresh it.

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

## Datasheet check

Checked on 2026-11-06 against BST-BMP388-DS001-07 Revision_1.7_112020. Not run on hardware.

- **Matched:** Addresses, chip ID, register addresses, PWR_CTRL and STATUS bits, OSR field positions, the 6-byte data read, the calibration block and its float conversions, temperature and pressure compensation, the poll limit.
- **Fixed:** OSR received the enum numbers instead of the manual's codes, so every setting ran one step above its name and x32 wrote 110, which the manual does not define. Comments cited section 9.3 for temperature, which is in 9.2. `BMP388_OVERSAMPLING_SKIP` did not skip: OSR has no skip code, so it measured at x1 with both channels on. It now clears press_en or temp_en, and the poll waits for the flags of what was enabled, so with both enabled it waits for both instead of drdy_press alone. The model and mock started STATUS at 0x20 where the manual gives 0x00, and the model ignored the enable bits and handled neither flag properly, so the tests could not see the poll loop. This fix has not run on hardware. `bmp388_reset` returned at once and left OSR at its default while `dev->cfg` kept the old value, so the next read ran at other settings with no error; it now waits for por_detected and writes OSR back. The model takes the manual's conversion time, returns to sleep after a measurement, restores Table 24 on a soft reset and ignores writes to read-only registers, and the mock raises STATUS only after a forced measurement.
- **Still open:** ERR_REG is never read, so sensor errors go unseen. With a NULL `delay_ms` the 500 polls can end before an x32 measurement does, and the read times out. Normal mode, ODR, the IIR filter, FIFO, interrupts, sensor time and SPI are not implemented.
- **Suspected:** `bmp388_reset` does not check cmd_rdy first. The manual gives no soft reset time, so the 2 ms in the driver and the model is its start-up time, and only hardware can show whether the chip answers on the bus during a reset.

## Known issues

- Uses forced mode per read (not continuous mode).
- I2C only; SPI not implemented.

## How to test

```bash
cd wheels/drivers/bmp388/test
cmake -GNinja -S . -B build && cmake --build build
./build/test_bmp388
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
