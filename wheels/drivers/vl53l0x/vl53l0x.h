/*
 * vl53l0x — ST VL53L0X I2C time-of-flight distance sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    i2c_hal (injected)
 *
 * Simplified init sequence (not the full ST API). Single-shot ranging
 * with 16-bit millimeter output. Pure C99, no heap, fully reentrant.
 */

#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VL53L0X_OK           =  0,
    VL53L0X_ERR_I2C      = -1,
    VL53L0X_ERR_ID       = -2,
    VL53L0X_ERR_PARAM    = -3,
    VL53L0X_ERR_TIMEOUT  = -4,
    VL53L0X_ERR_RANGE    = -5,
} vl53l0x_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    /** Optional: delay in ms; used to pace polling loops. May be NULL. */
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx;
} vl53l0x_i2c_t;

typedef struct {
    uint8_t addr;
} vl53l0x_cfg_t;

typedef struct {
    vl53l0x_i2c_t  i2c;
    vl53l0x_cfg_t  cfg;
    uint32_t       timing_budget_us;
} vl53l0x_t;

#define VL53L0X_ADDR_DEFAULT  0x29u
#define VL53L0X_MODEL_ID      0xEEu

/** Initialize and verify model ID. @thread_safe: no */
vl53l0x_err_t vl53l0x_init(vl53l0x_t *dev, const vl53l0x_i2c_t *i2c,
                            const vl53l0x_cfg_t *cfg);

/** Read range in millimeters (blocking single-shot). @thread_safe: no */
vl53l0x_err_t vl53l0x_read_range_mm(vl53l0x_t *dev, uint16_t *range_mm);

/** Start a single-shot measurement (non-blocking). @thread_safe: no */
vl53l0x_err_t vl53l0x_start_single(vl53l0x_t *dev);

/** Check if measurement result is ready. @thread_safe: no */
vl53l0x_err_t vl53l0x_is_ready(vl53l0x_t *dev, bool *ready);

/** Set measurement timing budget in microseconds. @thread_safe: no */
vl53l0x_err_t vl53l0x_set_measurement_timing_budget(vl53l0x_t *dev,
                                                     uint32_t budget_us);

#ifdef __cplusplus
}
#endif

#endif /* VL53L0X_H */
