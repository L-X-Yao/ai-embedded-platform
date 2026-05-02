# QMC5883P — 3-axis Magnetometer Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for QST QMC5883P digital magnetometer.

## Origin

Extracted from `projects/flybox/src/drivers/mag/mag_driver.c`. Flybox retains soft I2C bit-bang (PA11/PA12 open-drain) + bus recovery + 3x retry wrapper.

## Why self-written?

Searched GitHub (2026-05-02). No platform-agnostic pure C driver found. QMC5883P different from QMC5883L; no matching driver. Self-written with HAL injection per platform conventions.

## Dependencies

None. Pure C99 with injected I2C HAL callbacks.

## API

```c
qmc5883p_err_t  qmc5883p_init(const qmc5883p_i2c_t *i2c);
qmc5883p_err_t  qmc5883p_read(const qmc5883p_i2c_t *i2c, qmc5883p_data_t *out);
```

Caller provides I2C HAL via `qmc5883p_i2c_t` callbacks (write_reg + read_regs).

## Supported platforms

- [x] Host C99 (mock I2C) — 20 tests, 45 assertions, 2026-04-29
- [x] GD32F405 + QMC5883P @ soft I2C (PA11/PA12, open-drain) — HIL verified 2026-04-27

## Known issues

- QMC5883P I2C locks up if MCU resets mid-transaction. Software bus recovery is ineffective — only power cycle (>= 3 seconds) works. Consider GPIO-controlled MOSFET on VCC for automated recovery.
- Software I2C must use open-drain GPIO. Push-pull causes ACK to work but data reads return 0xFF (bus contention during data phase).
- No configurable ODR/range/oversampling yet — init hard-codes 200 Hz / 2G / 512x. Sufficient for compass use cases; extend if needed.

## How to test

```bash
cd wheels/drivers/qmc5883p/test
cmake -GNinja -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Test status

- Host mock tests: PASS (20 tests, 45 assertions — init sequence, wake register, mid-init failure at soft-reset/SIGN/CR2/CR1, chip ID validation, read path, field conversion, data-not-ready handling, I2C error propagation, NULL safety, re-init idempotency)
- HIL: PASS on GD32F405 + QMC5883P (software I2C, PA11 SCL / PA12 SDA, open-drain), 2026-04-27

## API convention deviations

None.

## API review

- Date: 2026-04-27
- Reviewer: @user
- Key decisions: wake register (0x0D) hidden inside init (not exposed to caller); output in Gauss not raw counts; minimal 2-function API (init + read)

## Responsible human

@user

## Features

- WHOAMI register check (0x80 @ reg 0x00)
- Hidden wake register (0x0D <- 0x40) handled internally by `init` — caller does not need to pre-wake the chip
- Continuous measurement mode (200 Hz ODR, 2 Gauss range, 512x oversampling)
- 3-axis magnetic field output in Gauss (float, sensitivity 15000 LSB/G)
- Data ready flag check before read
- I2C HAL injection via callback struct — zero RTOS dependency

## Usage

```c
static bool my_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val) { ... }
static bool my_read_regs(void *ctx, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) { ... }

qmc5883p_i2c_t i2c = { .write_reg = my_write_reg, .read_regs = my_read_regs, .ctx = NULL };

if (qmc5883p_init(&i2c) == QMC5883P_OK) {
    qmc5883p_data_t data;
    if (qmc5883p_read(&i2c, &data) == QMC5883P_OK) {
        /* data.mx_gauss/my_gauss/mz_gauss in Gauss (range +-2G) */
    }
}
```

## Upgrade to stable

- [ ] 2nd project consumer (currently only flybox)
- [ ] Configurable ODR/range/oversampling if 2nd consumer needs different settings
- [ ] Performance reference (read latency, I2C transaction count)
- [ ] Change history
