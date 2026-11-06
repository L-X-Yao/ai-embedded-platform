# QMC5883P — 3-axis Magnetometer Driver

**Status: candidate**
**Layer: L1 (drivers)**

Platform-agnostic driver for QST QMC5883P digital magnetometer.

## Origin

Reimplemented for the needs of an internal project. Soft I2C bit-bang, bus recovery and retry wrappers stay in the consumer.

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

## Usage

```c
static bool my_write_reg(void *ctx, uint8_t addr, uint8_t reg, uint8_t val)
    { (void)ctx; (void)addr; (void)reg; (void)val; return true; }
static bool my_read_regs(void *ctx, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
    { (void)ctx; (void)addr; (void)reg; (void)buf; (void)len; return true; }

static void example(void) {
    qmc5883p_i2c_t i2c = { .write_reg = my_write_reg, .read_regs = my_read_regs, .ctx = NULL };

    if (qmc5883p_init(&i2c) == QMC5883P_OK) {
        qmc5883p_data_t data;
        if (qmc5883p_read(&i2c, &data) == QMC5883P_OK) {
            /* data.mx_gauss/my_gauss/mz_gauss in Gauss (range +-2G) */
        }
    }
}
```

## Supported platforms

- [x] Host C99 (mock I2C) — 20 tests, 45 assertions, 2026-04-29
- [x] GD32F405 + QMC5883P @ soft I2C (PA11/PA12, open-drain) — HIL verified 2026-04-27

## Datasheet check

Checked on 2026-11-06 against the QST QMC5883P Datasheet QST-PD-B002-22, Rev E. This check was not run on hardware. The driver was not changed: the owner decided it is checked but not fixed, because no board is available to retest it.

- **Matched:** Address 0x2C, chip ID 0x80, data registers and byte order, the DRDY check, CR1 and CR2 layout, the +/-2 G range and 15000 LSB/G, SIGN 0x29 = 0x06 and the write order of the manual's example.
- **Fixed:** The model and mock never cleared DRDY on a status read, so a second read returned old data as new. A status read now clears DRDY, and in the model OVFL too.
- **Still open:** The chip has no 512x setting, so `CR1_OSR_512X` and the "512x" in this README are wrong: OSR1 = 00 means a ratio of 8. The register value is valid, so the chip is unaffected and only the name misleads.
- **Suspected:** Init writes 0x40 to register 0x0D, the "hidden wake register" above, which the register map does not have; the board ran with that write, so it may be needed for bring-up. The manual ties ODR to Normal mode and says Continuous mode runs flat out, so the real rate may not be 200 Hz. `qmc5883p_read` tests only DRDY, and reading the status clears OVFL, so data past +/-30000 LSB (about 2 G) comes back as OK.

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

## Upgrade to stable

- [ ] 2nd project consumer (currently only flybox)
- [ ] Configurable ODR/range/oversampling if 2nd consumer needs different settings
- [ ] Performance reference (read latency, I2C transaction count)
- [ ] Change history

## Features

- WHOAMI register check (0x80 @ reg 0x00)
- Hidden wake register (0x0D <- 0x40) handled internally by `init` — caller does not need to pre-wake the chip
- Continuous measurement mode (200 Hz ODR, 2 Gauss range, 512x oversampling)
- 3-axis magnetic field output in Gauss (float, sensitivity 15000 LSB/G)
- Data ready flag check before read
- I2C HAL injection via callback struct — zero RTOS dependency

## API review

- Date: 2026-04-27
- Reviewer: @user
- Key decisions: wake register (0x0D) hidden inside init (not exposed to caller); output in Gauss not raw counts; minimal 2-function API (init + read)

## Responsible human

@user
