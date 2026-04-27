# QMC5883P — 3-axis Magnetometer Driver (L1)

Platform-agnostic driver for QST QMC5883P digital magnetometer.

## Why not VSF native?

VSF has no magnetometer/compass component. Searched `vsf/source/{component,service,hal}/` — zero hits for `qmc5883`, `mag`, `compass`, `magnetometer`.

## API

```c
qmc5883p_err_t  qmc5883p_init(const qmc5883p_i2c_t *i2c);
qmc5883p_err_t  qmc5883p_read(const qmc5883p_i2c_t *i2c, qmc5883p_data_t *out);
```

Caller provides I2C HAL via `qmc5883p_i2c_t` callbacks. No RTOS dependency.

Init handles the hidden 0x0D wake register internally — caller does not need to pre-wake the chip.

## Usage

```c
static bool my_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val) { ... }
static bool my_read_regs(void *ctx, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) { ... }

qmc5883p_i2c_t i2c = { .write_reg = my_write_reg, .read_regs = my_read_regs, .ctx = NULL };

if (qmc5883p_init(&i2c) == QMC5883P_OK) {
    qmc5883p_data_t data;
    if (qmc5883p_read(&i2c, &data) == QMC5883P_OK) {
        /* data.mx/my/mz in Gauss (range +-2G, sensitivity 15000 LSB/G) */
    }
}
```

## Known Issues

- QMC5883P I2C locks up if MCU resets mid-transaction. Software bus recovery is ineffective — only power cycle works. Consider GPIO-controlled MOSFET on VCC for automated recovery.
- Software I2C must use open-drain GPIO. Push-pull causes ACK to work but data reads return 0xFF.

## Origin

Extracted from `projects/flybox/` QMC5883P driver. HIL verified on GD32F405 + QMC5883P @ soft I2C (PA11/PA12).

## Test

```bash
cd test && mkdir build && cd build
cmake .. && make && ./test_qmc5883p
```
