// SPDX-License-Identifier: MIT
/*
 * ccs811 — AMS/ScioSense CCS811 digital eCO2/TVOC gas sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * I2C interface injected via callbacks. Supports drive mode selection
 * (idle, 1sec, 10sec, 60sec measurement intervals).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef CCS811_H
#define CCS811_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    CCS811_OK          =  0,
    CCS811_ERR_I2C     = -1,
    CCS811_ERR_ID      = -2,
    CCS811_ERR_PARAM   = -3,
    CCS811_ERR_APP     = -4,   /* APP not valid */
    CCS811_ERR_NO_DATA = -5,   /* data not ready */
} ccs811_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ccs811_i2c_t;

/* ---------- drive mode ---------- */

typedef enum {
    CCS811_MODE_IDLE  = 0,
    CCS811_MODE_1SEC  = 1,
    CCS811_MODE_10SEC = 2,
    CCS811_MODE_60SEC = 3,
} ccs811_mode_t;

/* ---------- constants ---------- */

#define CCS811_DEV_ADDR      0x5A
#define CCS811_DEV_ADDR_ALT  0x5B
#define CCS811_HW_ID         0x81

/* ---------- configuration ---------- */

typedef struct {
    uint8_t       addr;   /* I2C address (0x5A or 0x5B) */
    ccs811_mode_t mode;   /* measurement drive mode */
} ccs811_cfg_t;

/* ---------- data types ---------- */

typedef struct {
    uint16_t eco2_ppm;    /* equivalent CO2 in ppm (400-8192) */
    uint16_t tvoc_ppb;    /* total VOC in ppb (0-1187) */
} ccs811_data_t;

/* ---------- device context ---------- */

typedef struct {
    const ccs811_i2c_t *i2c;
    ccs811_cfg_t        cfg;
} ccs811_t;

/* ---------- API ---------- */

/** Verify HW ID, start app mode, set drive mode. @thread_safe: no @isr_safe: no */
ccs811_err_t ccs811_init(ccs811_t *dev, const ccs811_i2c_t *i2c,
                         const ccs811_cfg_t *cfg);

/** Read eCO2/TVOC data (returns ERR_NO_DATA if not ready). @thread_safe: caller_must_lock @isr_safe: no */
ccs811_err_t ccs811_read(ccs811_t *dev, ccs811_data_t *out);

/** Set environmental data for compensation. @thread_safe: no @isr_safe: no */
ccs811_err_t ccs811_set_env_data(ccs811_t *dev, float humidity_pct,
                                 float temperature_c);

#ifdef __cplusplus
}
#endif

#endif /* CCS811_H */
