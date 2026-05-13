// SPDX-License-Identifier: MIT
/*
 * pct2075 — NXP PCT2075 digital temperature sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 11-bit temperature resolution (0.125 C steps), range -55 C to +125 C.
 * LM75 successor with higher resolution and programmable idle time.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef PCT2075_H
#define PCT2075_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    PCT2075_OK        =  0,
    PCT2075_ERR_I2C   = -1,
    PCT2075_ERR_PARAM = -2,
} pct2075_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} pct2075_i2c_t;

/* ---------- constants ---------- */

#define PCT2075_DEV_ADDR  0x48u

/* ---------- configuration ---------- */

typedef struct {
    uint8_t addr;           /* I2C address (0x48-0x4F) */
    bool    write_config;   /* if true, write conf_val to config register */
    uint8_t conf_val;       /* configuration register value to write */
} pct2075_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const pct2075_i2c_t *i2c;
    uint8_t addr;
} pct2075_t;

/* ---------- API ---------- */

/** Initialize device, store config, optionally write config register.
 *  Verifies I2C communication by reading the temperature register.
 *  @thread_safe: no @isr_safe: no */
pct2075_err_t pct2075_init(pct2075_t *dev, const pct2075_i2c_t *i2c,
                            const pct2075_cfg_t *cfg);

/** Read temperature in degrees Celsius (0.125 C resolution).
 *  @thread_safe: no @isr_safe: no */
pct2075_err_t pct2075_read(pct2075_t *dev, float *temp_c);

#ifdef __cplusplus
}
#endif

#endif /* PCT2075_H */
