// SPDX-License-Identifier: MIT
/*
 * sk6812 — TODO: one-line description
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      gpio_hal (injected)
 * Instances: multi
 */

#ifndef SK6812_H
#define SK6812_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    SK6812_OK          =  0,
    SK6812_ERR_PARAM   = -1,
    SK6812_ERR_TIMEOUT = -2,
} sk6812_err_t;

/* ---------- GPIO HAL (caller provides) ---------- */

typedef struct {
    void (*set_output)(void *ctx);
    void (*set_input)(void *ctx);
    void (*write_pin)(void *ctx, bool level);
    bool (*read_pin)(void *ctx);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} sk6812_gpio_t;

/* ---------- device context ---------- */

typedef struct {
    const sk6812_gpio_t *gpio;  /* caller-owned, must outlive dev */
} sk6812_t;

/* ---------- API ---------- */

/** Initialize device. @thread_safe: no @isr_safe: no */
sk6812_err_t sk6812_init(sk6812_t *dev, const sk6812_gpio_t *gpio);

#ifdef __cplusplus
}
#endif

#endif /* SK6812_H */
