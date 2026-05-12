// SPDX-License-Identifier: MIT
/*
 * lm75 — LM75/LM75A/LM75B I2C digital temperature sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * 9-bit temperature resolution (0.5 C steps), range -55 C to +125 C.
 * Supports overtemperature shutdown (T_OS) and hysteresis (T_HYST)
 * threshold registers. Pure C99, no heap, fully reentrant per-context.
 */

#ifndef LM75_H
#define LM75_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    LM75_OK        =  0,
    LM75_ERR_I2C   = -1,
    LM75_ERR_PARAM = -2,
} lm75_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    bool (*write_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                       const uint8_t *buf, uint16_t len);
    void *ctx;
} lm75_i2c_t;

/* ---------- constants ---------- */

#define LM75_DEV_ADDR  0x48u

/* ---------- device context ---------- */

typedef struct {
    const lm75_i2c_t *i2c;
    uint8_t addr;
} lm75_t;

/* ---------- API ---------- */

/** Initialize with default address (0x48). @thread_safe: no @isr_safe: no */
lm75_err_t lm75_init(lm75_t *dev, const lm75_i2c_t *i2c);

/** Initialize with custom address (0x48-0x4F). @thread_safe: no @isr_safe: no */
lm75_err_t lm75_init_addr(lm75_t *dev, const lm75_i2c_t *i2c, uint8_t addr);

/** Read temperature in degrees Celsius. @thread_safe: no @isr_safe: no */
lm75_err_t lm75_read_temp(lm75_t *dev, float *temp_c);

/** Read raw 16-bit temperature register value. @thread_safe: no @isr_safe: no */
lm75_err_t lm75_read_raw(lm75_t *dev, int16_t *raw);

/** Set overtemperature shutdown threshold. @thread_safe: no @isr_safe: no */
lm75_err_t lm75_set_os_temp(lm75_t *dev, float temp_c);

/** Set hysteresis threshold. @thread_safe: no @isr_safe: no */
lm75_err_t lm75_set_hyst_temp(lm75_t *dev, float temp_c);

#ifdef __cplusplus
}
#endif

#endif /* LM75_H */
