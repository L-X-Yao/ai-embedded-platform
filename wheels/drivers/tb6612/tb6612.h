// SPDX-License-Identifier: MIT
/*
 * tb6612 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#ifndef TB6612_H
#define TB6612_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    TB6612_OK          =  0,
    TB6612_ERR_PARAM   = -1,
    TB6612_ERR_TIMEOUT = -2,
} tb6612_err_t;

/* ---------- GPIO HAL (caller provides) ---------- */

typedef struct {
    void (*set_output)(void *ctx);
    void (*set_input)(void *ctx);
    void (*write_pin)(void *ctx, bool level);
    bool (*read_pin)(void *ctx);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} tb6612_gpio_t;

/* ---------- device context ---------- */

typedef struct {
    const tb6612_gpio_t *gpio;  /* caller-owned, must outlive dev */
} tb6612_t;

/* ---------- API ---------- */

/** Initialize device. @thread_safe: no @isr_safe: no */
tb6612_err_t tb6612_init(tb6612_t *dev, const tb6612_gpio_t *gpio);

#ifdef __cplusplus
}
#endif

#endif /* TB6612_H */
