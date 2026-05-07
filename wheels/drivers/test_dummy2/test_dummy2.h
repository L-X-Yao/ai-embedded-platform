// SPDX-License-Identifier: MIT
/*
 * test_dummy2 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 */

#ifndef TEST_DUMMY2_H
#define TEST_DUMMY2_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    TEST_DUMMY2_OK        =  0,
    TEST_DUMMY2_ERR_I2C   = -1,
    TEST_DUMMY2_ERR_ID    = -2,
    TEST_DUMMY2_ERR_PARAM = -3,
} test_dummy2_err_t;

/* ---------- I2C HAL (caller provides) ---------- */

typedef struct {
    bool (*write_reg)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val);
    bool (*read_regs)(void *ctx, uint8_t dev_addr, uint8_t reg,
                      uint8_t *buf, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);   /* optional, NULL = not needed */
    void *ctx;
} test_dummy2_i2c_t;

/* ---------- constants ---------- */

#define TEST_DUMMY2_DEV_ADDR  0x42

/* ---------- data types ---------- */

typedef struct {
    float x, y, z;  /* TODO: use unit suffixes (_c, _pa, _mg, ...) */
} test_dummy2_data_t;

/* ---------- device context ---------- */

typedef struct {
    const test_dummy2_i2c_t *i2c;
} test_dummy2_t;

/* ---------- API ---------- */

/** Configure and verify chip ID. @thread_safe: no @isr_safe: no */
test_dummy2_err_t test_dummy2_init(test_dummy2_t *dev,
                               const test_dummy2_i2c_t *i2c);

/** Read sensor data. @thread_safe: caller_must_lock @isr_safe: no */
test_dummy2_err_t test_dummy2_read(test_dummy2_t *dev,
                               test_dummy2_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TEST_DUMMY2_H */
