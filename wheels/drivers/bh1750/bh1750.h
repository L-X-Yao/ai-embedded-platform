/*
 * bh1750 — ROHM BH1750FVI ambient light sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * Command-based I2C: write command byte, read 2 bytes.
 * NOT register-based (no register address in reads).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef BH1750_H
#define BH1750_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BH1750_OK         =  0,
    BH1750_ERR_I2C    = -1,
    BH1750_ERR_PARAM  = -2,
} bh1750_err_t;

typedef struct {
    bool (*write)(void *ctx, uint8_t dev_addr,
                  const uint8_t *data, uint16_t len);
    bool (*read)(void *ctx, uint8_t dev_addr,
                 uint8_t *data, uint16_t len);
    void *ctx;
} bh1750_i2c_t;

typedef enum {
    BH1750_CONT_H_RES   = 0x10,
    BH1750_CONT_H_RES2  = 0x11,
    BH1750_CONT_L_RES   = 0x13,
    BH1750_ONCE_H_RES   = 0x20,
    BH1750_ONCE_H_RES2  = 0x21,
    BH1750_ONCE_L_RES   = 0x23,
} bh1750_mode_t;

typedef struct {
    uint8_t      addr;
    bh1750_mode_t mode;
} bh1750_cfg_t;

typedef struct {
    bh1750_i2c_t i2c;
    bh1750_cfg_t cfg;
} bh1750_t;

#define BH1750_ADDR_LOW   0x23
#define BH1750_ADDR_HIGH  0x5C

bh1750_err_t bh1750_init(bh1750_t *dev, const bh1750_i2c_t *i2c,
                         const bh1750_cfg_t *cfg);

bh1750_err_t bh1750_read_lux(bh1750_t *dev, float *lux);

bh1750_err_t bh1750_power_on(bh1750_t *dev);

bh1750_err_t bh1750_power_off(bh1750_t *dev);

bh1750_err_t bh1750_reset(bh1750_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BH1750_H */
