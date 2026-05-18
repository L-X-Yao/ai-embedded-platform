# bmp388 -- Bosch BMP388 barometric pressure and temperature sensor driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for the Bosch BMP388 barometric pressure and
temperature sensor. I2C interface injected via callbacks. Implements
the Bosch float compensation algorithm from datasheet BST-BMP388-DS001
(Section 9.3) for temperature and pressure output.

## Origin

Proactive build. BMP388 is the successor to BMP280 with higher accuracy
and lower noise, used in drones, weather stations, and altimeters.

## Why not VSF native?

No VSF component for BMP388. Self-written with compensation formulas
cross-validated against the Bosch datasheet.

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

## Supported platforms

- [x] Host (sim)
- [ ] ARM Cortex-M -- not yet verified (uses injected I2C HAL)

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

## Responsible human

@user
