// SPDX-License-Identifier: MIT
/*
 * cst816s — Hynitron CST816S capacitive touch controller (I2C)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Single-point touch with gesture recognition for small displays
 * (smartwatches, wearables). I2C address 0x15, chip ID 0xB5.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef CST816S_H
#define CST816S_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    CST816S_OK        =  0,
    CST816S_ERR_I2C   = -1,
    CST816S_ERR_ID    = -2,
    CST816S_ERR_PARAM = -3,
} cst816s_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void *ctx;
} cst816s_i2c_t;

/* ---------- constants ---------- */

#define CST816S_DEV_ADDR   0x15u
#define CST816S_CHIP_ID    0xB5u

/* ---------- gesture IDs ---------- */

typedef enum {
    CST816S_GESTURE_NONE        = 0x00,
    CST816S_GESTURE_SLIDE_UP    = 0x01,
    CST816S_GESTURE_SLIDE_DOWN  = 0x02,
    CST816S_GESTURE_SLIDE_LEFT  = 0x03,
    CST816S_GESTURE_SLIDE_RIGHT = 0x04,
    CST816S_GESTURE_SINGLE_TAP  = 0x05,
    CST816S_GESTURE_DOUBLE_TAP  = 0x0B,
    CST816S_GESTURE_LONG_PRESS  = 0x0C,
} cst816s_gesture_t;

/* ---------- touch event types ---------- */

typedef enum {
    CST816S_EVENT_DOWN    = 0,
    CST816S_EVENT_UP      = 1,
    CST816S_EVENT_CONTACT = 2,
} cst816s_event_t;

/* ---------- data types ---------- */

typedef struct {
    uint16_t          x;
    uint16_t          y;
    cst816s_event_t   event;
    cst816s_gesture_t gesture;
    uint8_t           fingers;
} cst816s_touch_t;

/* ---------- device context ---------- */

typedef struct {
    const cst816s_i2c_t *i2c;
    uint8_t chip_id;
    uint8_t fw_version;
} cst816s_t;

/* ---------- API ---------- */

/** Configure and verify chip ID (0xA7 must read 0xB5).
 *  @thread_safe: no @isr_safe: no */
cst816s_err_t cst816s_init(cst816s_t *dev,
                           const cst816s_i2c_t *i2c);

/** Read current touch data (coordinates, event, gesture, finger count).
 *  @thread_safe: caller_must_lock @isr_safe: no */
cst816s_err_t cst816s_read(cst816s_t *dev,
                           cst816s_touch_t *out);

/** Enable or disable auto-sleep (register 0xFE).
 *  @thread_safe: no @isr_safe: no */
cst816s_err_t cst816s_set_auto_sleep(cst816s_t *dev, bool enable);

/** Configure motion detection mask (register 0xEC). The chip has no bit for
 *  long press, so long_press is ignored.
 *  @thread_safe: no @isr_safe: no */
cst816s_err_t cst816s_set_motion_mask(cst816s_t *dev,
                                      bool continuous,
                                      bool double_click,
                                      bool long_press);

/** Return cached chip ID (read during init). */
uint8_t cst816s_get_chip_id(const cst816s_t *dev);

/** Return cached firmware version (read during init). */
uint8_t cst816s_get_firmware_version(const cst816s_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* CST816S_H */
