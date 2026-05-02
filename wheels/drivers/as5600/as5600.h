// SPDX-License-Identifier: MIT
/*
 * as5600 — ams AS5600 I2C 12-bit magnetic rotary position sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Fixed I2C address 0x36. 12-bit raw angle (0-4095 maps to 0-360 degrees).
 * Supports reading raw angle, degrees, magnet status, and configuring
 * start position / max angle for narrowed output range.
 * Pure C99, no heap, fully reentrant per-context.
 */

#ifndef AS5600_H
#define AS5600_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AS5600_OK        =  0,
    AS5600_ERR_I2C   = -1,
    AS5600_ERR_PARAM = -2,
} as5600_err_t;

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} as5600_i2c_t;

/* AS5600 has a single fixed I2C address */
#define AS5600_ADDR  0x36u

/* Status register bit masks */
#define AS5600_STATUS_MH  0x08u  /* magnet too strong */
#define AS5600_STATUS_ML  0x10u  /* magnet too weak   */
#define AS5600_STATUS_MD  0x20u  /* magnet detected   */

typedef struct {
    const as5600_i2c_t *i2c;
} as5600_t;

/** Initialise driver context. @thread_safe: no @isr_safe: no */
as5600_err_t as5600_init(as5600_t *dev, const as5600_i2c_t *i2c);

/** Read 12-bit raw angle (0-4095). @thread_safe: caller_must_lock @isr_safe: no */
as5600_err_t as5600_read_raw_angle(const as5600_t *dev, uint16_t *raw);

/** Read angle in degrees (0.0-360.0). @thread_safe: caller_must_lock @isr_safe: yes */
as5600_err_t as5600_read_degrees(const as5600_t *dev, float *degrees);

/** Read STATUS register (use AS5600_STATUS_* masks). @thread_safe: caller_must_lock @isr_safe: no */
as5600_err_t as5600_read_status(const as5600_t *dev, uint8_t *status);

/** Set start position (ZPOS, 12-bit). @thread_safe: caller_must_lock @isr_safe: no */
as5600_err_t as5600_set_start_position(const as5600_t *dev, uint16_t pos);

/** Set maximum angle (MANG, 12-bit). @thread_safe: caller_must_lock @isr_safe: no */
as5600_err_t as5600_set_max_angle(const as5600_t *dev, uint16_t angle);

#ifdef __cplusplus
}
#endif

#endif /* AS5600_H */
