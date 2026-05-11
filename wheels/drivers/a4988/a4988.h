// SPDX-License-Identifier: MIT
/*
 * a4988 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#ifndef A4988_H
#define A4988_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    A4988_OK          =  0,
    A4988_ERR_PARAM   = -1,
    A4988_ERR_TIMEOUT = -2,
} a4988_err_t;

/* ---------- GPIO HAL (caller provides) ---------- */

typedef struct {
    void (*set_output)(void *ctx);
    void (*set_input)(void *ctx);
    void (*write_pin)(void *ctx, bool level);
    bool (*read_pin)(void *ctx);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} a4988_gpio_t;

/* ---------- device context ---------- */

typedef struct {
    const a4988_gpio_t *gpio;  /* caller-owned, must outlive dev */
} a4988_t;

/* ---------- API ---------- */

/** Initialize device. @thread_safe: no @isr_safe: no */
a4988_err_t a4988_init(a4988_t *dev, const a4988_gpio_t *gpio);

#ifdef __cplusplus
}
#endif

#endif /* A4988_H */
