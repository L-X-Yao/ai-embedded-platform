// SPDX-License-Identifier: MIT
/*
 * soft_i2c — GPIO bit-bang I2C master with bus recovery
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SOFT_I2C_OK        =  0,
    SOFT_I2C_ERR_PARAM = -1,
    SOFT_I2C_ERR_NACK  = -2,
    SOFT_I2C_ERR_BUS   = -3,
} soft_i2c_err_t;

/* ---------- GPIO HAL injection ---------- */

typedef struct {
    void (*scl_set)(void *ctx, bool high);
    void (*sda_set)(void *ctx, bool high);
    void (*sda_dir_out)(void *ctx);
    void (*sda_dir_in)(void *ctx);
    bool (*sda_get)(void *ctx);
    void (*delay)(void *ctx);
    void *ctx;
} soft_i2c_gpio_t;

/* ---------- instance ---------- */

typedef struct {
    const soft_i2c_gpio_t *gpio; /* internal */
} soft_i2c_t;

/* ---------- API ---------- */

/** @thread_safe: no (caller must serialize) @isr_safe: no */
soft_i2c_err_t soft_i2c_init(soft_i2c_t *bus, const soft_i2c_gpio_t *gpio);

/** @thread_safe: no @isr_safe: no */
soft_i2c_err_t soft_i2c_write(soft_i2c_t *bus, uint8_t addr,
                              const uint8_t *data, uint16_t len);

/** @thread_safe: no @isr_safe: no */
soft_i2c_err_t soft_i2c_read(soft_i2c_t *bus, uint8_t addr,
                             uint8_t *buf, uint16_t len);

/** @thread_safe: no @isr_safe: no */
soft_i2c_err_t soft_i2c_write_reg(soft_i2c_t *bus, uint8_t addr,
                                  uint8_t reg, uint8_t val);

/** @thread_safe: no @isr_safe: no */
soft_i2c_err_t soft_i2c_read_regs(soft_i2c_t *bus, uint8_t addr,
                                  uint8_t reg, uint8_t *buf, uint16_t len);

/** @thread_safe: no @isr_safe: no */
soft_i2c_err_t soft_i2c_bus_recover(soft_i2c_t *bus);

#ifdef __cplusplus
}
#endif

#endif /* SOFT_I2C_H */
