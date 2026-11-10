// SPDX-License-Identifier: MIT
/*
 * ltr390 — Lite-On LTR-390UV UV and ambient light sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 20-bit ALS and UVS channels with configurable gain (1x-18x) and
 * resolution (13-bit to 20-bit). I2C address fixed at 0x53.
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the ltr390_t struct.
 */

#ifndef LTR390_H
#define LTR390_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    LTR390_OK        =  0,
    LTR390_ERR_I2C   = -1,
    LTR390_ERR_ID    = -2,
    LTR390_ERR_PARAM = -3,
    LTR390_ERR_NOT_READY = -4,  /* no current result for this channel, see ltr390_read_als */
} ltr390_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} ltr390_i2c_t;

/* ---------- constants ---------- */

#define LTR390_DEV_ADDR       0x53u
#define LTR390_PART_ID_VAL    0xB2u   /* bits [7:4]=0xB part, [3:0]=rev */

/* ---------- enums ---------- */

/** Measurement mode: ambient light or UV. */
typedef enum {
    LTR390_MODE_ALS = 0,    /* ambient light */
    LTR390_MODE_UVS = 1,    /* UV */
} ltr390_mode_t;

/** Gain settings. */
typedef enum {
    LTR390_GAIN_1  = 0,
    LTR390_GAIN_3  = 1,
    LTR390_GAIN_6  = 2,
    LTR390_GAIN_9  = 3,
    LTR390_GAIN_18 = 4,
} ltr390_gain_t;

/** Resolution (integration time) settings. */
typedef enum {
    LTR390_RES_20BIT = 0,   /* 400 ms */
    LTR390_RES_19BIT = 1,   /* 200 ms */
    LTR390_RES_18BIT = 2,   /* 100 ms (default) */
    LTR390_RES_17BIT = 3,   /* 50 ms */
    LTR390_RES_16BIT = 4,   /* 25 ms */
    LTR390_RES_13BIT = 5,   /* 12.5 ms */
} ltr390_res_t;

/* ---------- configuration ---------- */

typedef struct {
    ltr390_mode_t mode;
    ltr390_gain_t gain;
    ltr390_res_t  resolution;
} ltr390_cfg_t;

/* ---------- device context ---------- */

typedef struct {
    const ltr390_i2c_t *i2c;
    ltr390_mode_t       mode;
    bool                have_data;  /* internal: a conversion ended in this mode */
} ltr390_t;

/* ---------- API ---------- */

/** Configure and verify part ID. @thread_safe: no @isr_safe: no */
ltr390_err_t ltr390_init(ltr390_t *dev, const ltr390_i2c_t *i2c,
                         const ltr390_cfg_t *cfg);

/**
 * Read ambient light raw value (20-bit). @thread_safe: caller_must_lock @isr_safe: no
 *
 * LTR390_ERR_NOT_READY, with raw left alone, until the first conversion has ended
 * after ltr390_init or ltr390_set_mode, and always while the sensor is in the other
 * mode. Once a result exists, reads between conversions return it with LTR390_OK.
 */
ltr390_err_t ltr390_read_als(ltr390_t *dev, uint32_t *raw);

/** Read UV raw value (20-bit), as ltr390_read_als. @thread_safe: caller_must_lock @isr_safe: no */
ltr390_err_t ltr390_read_uvs(ltr390_t *dev, uint32_t *raw);

/** Switch measurement mode (ALS or UVS). @thread_safe: caller_must_lock @isr_safe: no */
ltr390_err_t ltr390_set_mode(ltr390_t *dev, ltr390_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* LTR390_H */
