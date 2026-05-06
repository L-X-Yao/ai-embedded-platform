// SPDX-License-Identifier: MIT
/*
 * tsl2591 — AMS TSL2591 high-dynamic-range ambient light sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register-based I2C with command byte prefix (0xA0 | reg).
 * Dual channel (visible + IR) with 600M:1 dynamic range.
 * Configurable gain (1x..9876x) and integration time (100ms..600ms).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: caller owns the tsl2591_t struct.
 */

#ifndef TSL2591_H
#define TSL2591_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    TSL2591_OK        =  0,
    TSL2591_ERR_I2C   = -1,
    TSL2591_ERR_ID    = -2,
    TSL2591_ERR_PARAM = -3,
} tsl2591_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} tsl2591_i2c_t;

/* ---------- constants ---------- */

#define TSL2591_DEV_ADDR   0x29u
#define TSL2591_DEVICE_ID  0x50u

/* ---------- register addresses ---------- */

#define TSL2591_REG_ENABLE   0x00u
#define TSL2591_REG_CONFIG   0x01u
#define TSL2591_REG_ID       0x12u
#define TSL2591_REG_STATUS   0x13u
#define TSL2591_REG_C0DATAL  0x14u
#define TSL2591_REG_C0DATAH  0x15u
#define TSL2591_REG_C1DATAL  0x16u
#define TSL2591_REG_C1DATAH  0x17u

/* Command byte: COMMAND bit (7) + NORMAL transaction (5) */
#define TSL2591_CMD_BIT      0xA0u

/* ---------- gain enum ---------- */

typedef enum {
    TSL2591_GAIN_LOW    = 0x00,   /*    1x */
    TSL2591_GAIN_MEDIUM = 0x10,   /*   25x */
    TSL2591_GAIN_HIGH   = 0x20,   /*  428x */
    TSL2591_GAIN_MAX    = 0x30,   /* 9876x */
} tsl2591_gain_t;

/* ---------- integration time enum ---------- */

typedef enum {
    TSL2591_ITIME_100MS = 0x00,
    TSL2591_ITIME_200MS = 0x01,
    TSL2591_ITIME_300MS = 0x02,
    TSL2591_ITIME_400MS = 0x03,
    TSL2591_ITIME_500MS = 0x04,
    TSL2591_ITIME_600MS = 0x05,
} tsl2591_itime_t;

/* ---------- data types ---------- */

typedef struct {
    float     lux;         /* computed illuminance (lux) */
    uint16_t  ch0_raw;     /* channel 0: visible + IR    */
    uint16_t  ch1_raw;     /* channel 1: IR only         */
    uint32_t  visible;     /* ch0 - ch1 (clamped >= 0)   */
    uint16_t  infrared;    /* ch1 raw copy               */
} tsl2591_data_t;

/* ---------- device context ---------- */

typedef struct {
    const tsl2591_i2c_t *i2c;
    tsl2591_gain_t       gain;
    tsl2591_itime_t      itime;
} tsl2591_t;

/* ---------- API ---------- */

/** Configure and verify chip ID (expects 0x50). @thread_safe: no @isr_safe: no */
tsl2591_err_t tsl2591_init(tsl2591_t *dev, const tsl2591_i2c_t *i2c,
                           tsl2591_gain_t gain, tsl2591_itime_t itime);

/** Read sensor data (both channels + computed lux). @thread_safe: caller_must_lock @isr_safe: no */
tsl2591_err_t tsl2591_read(tsl2591_t *dev, tsl2591_data_t *out);

/** Read raw channel values without lux computation. @thread_safe: caller_must_lock @isr_safe: no */
tsl2591_err_t tsl2591_read_raw(tsl2591_t *dev, uint16_t *ch0, uint16_t *ch1);

/** Set gain. @thread_safe: caller_must_lock @isr_safe: no */
tsl2591_err_t tsl2591_set_gain(tsl2591_t *dev, tsl2591_gain_t gain);

/** Set integration time. @thread_safe: caller_must_lock @isr_safe: no */
tsl2591_err_t tsl2591_set_integration_time(tsl2591_t *dev, tsl2591_itime_t itime);

/** Enable sensor (AEN + PON). @thread_safe: caller_must_lock @isr_safe: no */
tsl2591_err_t tsl2591_enable(tsl2591_t *dev);

/** Disable sensor (clear AEN + PON). @thread_safe: caller_must_lock @isr_safe: no */
tsl2591_err_t tsl2591_disable(tsl2591_t *dev);

/** Read STATUS register. @thread_safe: caller_must_lock @isr_safe: no */
tsl2591_err_t tsl2591_get_status(tsl2591_t *dev, uint8_t *status);

#ifdef __cplusplus
}
#endif

#endif /* TSL2591_H */
