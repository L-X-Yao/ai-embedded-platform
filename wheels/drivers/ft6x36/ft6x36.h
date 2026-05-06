// SPDX-License-Identifier: MIT
/*
 * ft6x36 — FocalTech FT6236/FT6336 capacitive touch controller (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Supports 1-2 simultaneous touch points, gesture detection,
 * touch threshold configuration, and chip identification.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef FT6X36_H
#define FT6X36_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    FT6X36_OK        =  0,
    FT6X36_ERR_I2C   = -1,
    FT6X36_ERR_ID    = -2,
    FT6X36_ERR_PARAM = -3,
} ft6x36_err_t;

/* ---------- touch event types ---------- */

typedef enum {
    FT6X36_EVT_PRESS   = 0,
    FT6X36_EVT_RELEASE = 1,
    FT6X36_EVT_CONTACT = 2,
} ft6x36_event_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} ft6x36_i2c_t;

/* ---------- constants ---------- */

#define FT6X36_DEV_ADDR  0x38u

/* ---------- registers ---------- */

#define FT6X36_REG_DEV_MODE   0x00u
#define FT6X36_REG_GEST_ID    0x01u
#define FT6X36_REG_TD_STATUS  0x02u
#define FT6X36_REG_P1_XH      0x03u
#define FT6X36_REG_P1_XL      0x04u
#define FT6X36_REG_P1_YH      0x05u
#define FT6X36_REG_P1_YL      0x06u
#define FT6X36_REG_P2_XH      0x09u
#define FT6X36_REG_P2_XL      0x0Au
#define FT6X36_REG_P2_YH      0x0Bu
#define FT6X36_REG_P2_YL      0x0Cu
#define FT6X36_REG_TH_GROUP   0x80u
#define FT6X36_REG_FW_VER     0xA6u
#define FT6X36_REG_CHIP_ID    0xA8u

/* Expected chip ID values */
#define FT6X36_CHIP_ID_6236   0x36u
#define FT6X36_CHIP_ID_6336   0x64u

/* ---------- data types ---------- */

typedef struct {
    uint8_t         touch_count;    /* 0-2 active points */
    uint16_t        x1;
    uint16_t        y1;
    ft6x36_event_t  event1;
    uint16_t        x2;
    uint16_t        y2;
    ft6x36_event_t  event2;
} ft6x36_touch_t;

/* ---------- device context ---------- */

typedef struct {
    const ft6x36_i2c_t *i2c;
    uint8_t chip_id;
} ft6x36_t;

/* ---------- API ---------- */

/** Configure and verify chip ID (0xA8 must read 0x36 or 0x64).
 *  @thread_safe: no @isr_safe: no */
ft6x36_err_t ft6x36_init(ft6x36_t *dev, const ft6x36_i2c_t *i2c);

/** Read current touch data (0-2 points).
 *  @thread_safe: caller_must_lock @isr_safe: no */
ft6x36_err_t ft6x36_read_touch(ft6x36_t *dev, ft6x36_touch_t *out);

/** Set touch detection threshold (register 0x80).
 *  @thread_safe: no @isr_safe: no */
ft6x36_err_t ft6x36_set_threshold(ft6x36_t *dev, uint8_t value);

/** Read chip ID register (0xA8). Result stored in *out.
 *  @thread_safe: no @isr_safe: no */
ft6x36_err_t ft6x36_get_chip_id(ft6x36_t *dev, uint8_t *out);

/** Read firmware version register (0xA6). Result stored in *out.
 *  @thread_safe: no @isr_safe: no */
ft6x36_err_t ft6x36_get_firmware_version(ft6x36_t *dev, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* FT6X36_H */
