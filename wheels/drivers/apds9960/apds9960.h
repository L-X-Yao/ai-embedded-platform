// SPDX-License-Identifier: MIT
/*
 * apds9960 — I2C proximity, ambient light, RGB color, and gesture sensor
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Broadcom/Avago APDS-9960 driver. I2C interface injected via callbacks.
 * Fixed address 0x39. Supports proximity, RGBC color, and simple 4-dir
 * gesture detection from FIFO data (UP/DOWN/LEFT/RIGHT).
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef APDS9960_H
#define APDS9960_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APDS9960_OK          =  0,
    APDS9960_ERR_I2C     = -1,
    APDS9960_ERR_PARAM  = -2,
    APDS9960_ERR_ID      = -3,
} apds9960_err_t;

typedef enum {
    APDS9960_GESTURE_NONE  = 0,
    APDS9960_GESTURE_UP,
    APDS9960_GESTURE_DOWN,
    APDS9960_GESTURE_LEFT,
    APDS9960_GESTURE_RIGHT,
} apds9960_gesture_t;

typedef struct {
    uint16_t r, g, b, c;
} apds9960_color_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} apds9960_i2c_t;

typedef struct {
    const apds9960_i2c_t *i2c;
} apds9960_t;

#define APDS9960_DEV_ADDR  0x39u

/** Verify chip ID and power on. @thread_safe: no @isr_safe: no */
apds9960_err_t apds9960_init(apds9960_t *dev, const apds9960_i2c_t *i2c);

/** Enable proximity sensing. @thread_safe: caller_must_lock @isr_safe: no */
apds9960_err_t apds9960_enable_proximity(apds9960_t *dev);

/** Read 8-bit proximity value (0=far, 255=near). @thread_safe: caller_must_lock @isr_safe: no */
apds9960_err_t apds9960_read_proximity(apds9960_t *dev, uint8_t *prox);

/** Enable RGBC color sensing. @thread_safe: caller_must_lock @isr_safe: no */
apds9960_err_t apds9960_enable_color(apds9960_t *dev);

/** Read 16-bit RGBC color data. @thread_safe: caller_must_lock @isr_safe: no */
apds9960_err_t apds9960_read_color(apds9960_t *dev, apds9960_color_t *color);

/** Enable gesture engine. @thread_safe: caller_must_lock @isr_safe: no */
apds9960_err_t apds9960_enable_gesture(apds9960_t *dev);

/** Read gesture from FIFO. Returns GESTURE_NONE if no gesture. @thread_safe: caller_must_lock @isr_safe: no */
apds9960_err_t apds9960_read_gesture(apds9960_t *dev, apds9960_gesture_t *gesture);

/** Disable all features (power down). @thread_safe: caller_must_lock @isr_safe: no */
apds9960_err_t apds9960_disable_all(apds9960_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* APDS9960_H */
