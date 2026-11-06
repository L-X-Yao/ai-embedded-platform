// SPDX-License-Identifier: MIT
/*
 * adxl345 — Analog Devices ADXL345 3-axis accelerometer driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Reads 3-axis acceleration in
 * full-resolution mode (3.9 mg/LSB). Configurable range (2/4/8/16g).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADXL345_OK        =  0,
    ADXL345_ERR_I2C   = -1,
    ADXL345_ERR_ID    = -2,
    ADXL345_ERR_PARAM = -3,
} adxl345_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* NULL: see adxl345_init */
    void *ctx;
} adxl345_i2c_t;

typedef enum {
    ADXL345_RANGE_2G  = 0,
    ADXL345_RANGE_4G  = 1,
    ADXL345_RANGE_8G  = 2,
    ADXL345_RANGE_16G = 3,
} adxl345_range_t;

typedef enum {
    ADXL345_RATE_0_10HZ  = 0,
    ADXL345_RATE_0_20HZ  = 1,
    ADXL345_RATE_0_39HZ  = 2,
    ADXL345_RATE_0_78HZ  = 3,
    ADXL345_RATE_1_56HZ  = 4,
    ADXL345_RATE_3_13HZ  = 5,
    ADXL345_RATE_6_25HZ  = 6,
    ADXL345_RATE_12_5HZ  = 7,
    ADXL345_RATE_25HZ    = 8,
    ADXL345_RATE_50HZ    = 9,
    ADXL345_RATE_100HZ   = 10,
    ADXL345_RATE_200HZ   = 11,
    ADXL345_RATE_400HZ   = 12,
    ADXL345_RATE_800HZ   = 13,
    ADXL345_RATE_1600HZ  = 14,   /* SPI only: refused by this I2C driver */
    ADXL345_RATE_3200HZ  = 15,   /* SPI only: refused by this I2C driver */
} adxl345_data_rate_t;

typedef struct {
    uint8_t            addr;
    adxl345_range_t    range;
    adxl345_data_rate_t data_rate;
} adxl345_cfg_t;

typedef struct {
    float x_g;
    float y_g;
    float z_g;
} adxl345_data_t;

typedef struct {
    const adxl345_i2c_t *i2c;
    adxl345_cfg_t  cfg;
} adxl345_t;

#define ADXL345_ADDR_PRIMARY   0x53
#define ADXL345_ADDR_ALT       0x1D
#define ADXL345_CHIP_ID        0xE5

/** Configure the part and start measuring, then wait through delay_ms for the
 *  turn-on time, 1/(data rate) + 1.1 ms: 12 ms at 100 Hz, but 643 ms at 1.56 Hz
 *  and 10 s at 0.10 Hz. With no delay_ms the caller must wait that long before
 *  the first adxl345_read, which until then returns zeros.
 *  @thread_safe: no @isr_safe: no */
adxl345_err_t adxl345_init(adxl345_t *dev, const adxl345_i2c_t *i2c,
                           const adxl345_cfg_t *cfg);

/** @thread_safe: no @isr_safe: no */
adxl345_err_t adxl345_read(const adxl345_t *dev, adxl345_data_t *out);

/** @thread_safe: no @isr_safe: no */
adxl345_err_t adxl345_set_range(adxl345_t *dev, adxl345_range_t range);

#ifdef __cplusplus
}
#endif

#endif /* ADXL345_H */
