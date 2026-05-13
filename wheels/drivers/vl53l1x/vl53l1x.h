// SPDX-License-Identifier: MIT
/*
 * vl53l1x — ST VL53L1X time-of-flight distance sensor
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Simplified driver for the VL53L1X ToF sensor. Uses 16-bit register
 * addresses (unlike most I2C sensors). Continuous ranging mode with
 * 16-bit millimeter distance output, up to 4 meters.
 */

#ifndef VL53L1X_H
#define VL53L1X_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    VL53L1X_OK        =  0,
    VL53L1X_ERR_I2C   = -1,
    VL53L1X_ERR_ID    = -2,
    VL53L1X_ERR_PARAM = -3,
} vl53l1x_err_t;

/* ---------- I2C HAL (caller provides, 16-bit register addresses) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint16_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint16_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} vl53l1x_i2c_t;

/* ---------- configuration ---------- */

typedef struct {
    uint8_t addr;           /**< I2C 7-bit address (default 0x29) */
} vl53l1x_cfg_t;

/* ---------- constants ---------- */

#define VL53L1X_ADDR_DEFAULT   0x29u
#define VL53L1X_MODEL_ID_VAL   0xEAu
#define VL53L1X_MODULE_TYPE_VAL 0xCCu

/* ---------- device context ---------- */

typedef struct {
    const vl53l1x_i2c_t *i2c;
    vl53l1x_cfg_t         cfg;
    bool                   ranging;
} vl53l1x_t;

/* ---------- API ---------- */

/** Initialize and verify model ID + module type. @thread_safe: no @isr_safe: no */
vl53l1x_err_t vl53l1x_init(vl53l1x_t *dev, const vl53l1x_i2c_t *i2c,
                            const vl53l1x_cfg_t *cfg);

/** Read distance in millimeters. @thread_safe: caller_must_lock @isr_safe: no */
vl53l1x_err_t vl53l1x_read_distance(vl53l1x_t *dev, uint16_t *dist_mm);

/** Start continuous ranging. @thread_safe: no @isr_safe: no */
vl53l1x_err_t vl53l1x_start_ranging(vl53l1x_t *dev);

/** Stop continuous ranging. @thread_safe: no @isr_safe: no */
vl53l1x_err_t vl53l1x_stop_ranging(vl53l1x_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* VL53L1X_H */
