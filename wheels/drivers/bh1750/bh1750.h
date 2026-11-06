// SPDX-License-Identifier: MIT
/*
 * bh1750 — ROHM BH1750FVI ambient light sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Command-based I2C: write command byte, read 2 bytes.
 * NOT register-based (no register address in reads).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the bh1750_t struct.
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
    void (*delay_ms)(void *ctx, uint32_t ms);   /* ONCE modes need it: see bh1750_init */
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
    const bh1750_i2c_t *i2c;
    bh1750_cfg_t cfg;
} bh1750_t;

#define BH1750_ADDR_LOW   0x23
#define BH1750_ADDR_HIGH  0x5C

/** Power on and start cfg->mode. Calls delay_ms (max 180 ms, 24 ms in the L
 *  modes) so the first measurement has ended on return. With delay_ms NULL the
 *  caller must wait that long itself, or the first read gives 0 with BH1750_OK,
 *  and the ONCE modes return BH1750_ERR_PARAM, because nothing could wait inside
 *  bh1750_read_lux.
 *  @thread_safe: no @isr_safe: no */
bh1750_err_t bh1750_init(bh1750_t *dev, const bh1750_i2c_t *i2c,
                         const bh1750_cfg_t *cfg);

/** Read ambient light measurement in lux. A continuous mode reads the latest
 *  result and does not block. A ONCE mode measures first, as the chip powers
 *  down after each measurement, so it calls delay_ms (max 180 ms, 24 ms in the
 *  L mode). @thread_safe: caller_must_lock @isr_safe: no */
bh1750_err_t bh1750_read_lux(const bh1750_t *dev, float *lux);

/** Send power-on command to enable measurement. @thread_safe: caller_must_lock @isr_safe: no */
bh1750_err_t bh1750_power_on(const bh1750_t *dev);

/** Send power-down command to enter low-power mode. @thread_safe: caller_must_lock @isr_safe: no */
bh1750_err_t bh1750_power_off(const bh1750_t *dev);

/** Reset data register value; sensor must be powered on first. @thread_safe: no @isr_safe: no */
bh1750_err_t bh1750_reset(const bh1750_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* BH1750_H */
