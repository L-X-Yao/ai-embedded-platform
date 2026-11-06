// SPDX-License-Identifier: MIT
/*
 * soft_timer — tick-driven software timer manager
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Manages a static array of software timers driven by a periodic tick
 * (e.g., SysTick ISR). Supports one-shot and repeating modes with
 * per-timer callbacks. No heap, no framework dependency.
 * Thread safety: not thread-safe. Call tick from one context only.
 */

#ifndef SOFT_TIMER_H
#define SOFT_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SOFT_TIMER_MAX
#define SOFT_TIMER_MAX  16
#endif

typedef enum {
    SOFT_TIMER_OK        =  0,
    SOFT_TIMER_ERR_PARAM = -1,
    SOFT_TIMER_ERR_FULL  = -2,
} soft_timer_err_t;

typedef enum {
    SOFT_TIMER_ONE_SHOT = 0,
    SOFT_TIMER_REPEAT,
} soft_timer_mode_t;

/** Runs inside soft_timer_tick, in its context.  It may call soft_timer_start,
 *  stop, reset and is_active on any timer, its own included: a one-shot is
 *  already inactive by then, so it can re-arm itself.  create and destroy are
 *  also allowed, but only if tick does not run in an ISR. */
typedef void (*soft_timer_cb_t)(void *user_ctx);

typedef struct {
    uint32_t          period;
    uint32_t          remaining;
    soft_timer_cb_t   callback;
    void             *user_ctx;
    soft_timer_mode_t mode;
    bool              active;
} soft_timer_entry_t;

typedef struct {
    soft_timer_entry_t timers[SOFT_TIMER_MAX];
    size_t             count;
    uint32_t           tick_count;
} soft_timer_mgr_t;

/** @thread_safe: no @isr_safe: no */
soft_timer_err_t soft_timer_init(soft_timer_mgr_t *mgr);

/** @thread_safe: no @isr_safe: no */
int soft_timer_create(soft_timer_mgr_t *mgr, uint32_t period_ticks,
                      soft_timer_mode_t mode, soft_timer_cb_t cb,
                      void *user_ctx);

/** Release a timer slot for reuse. @thread_safe: no @isr_safe: no */
soft_timer_err_t soft_timer_destroy(soft_timer_mgr_t *mgr, int id);

/** @thread_safe: no @isr_safe: yes */
soft_timer_err_t soft_timer_start(soft_timer_mgr_t *mgr, int id);

/** @thread_safe: no @isr_safe: yes */
soft_timer_err_t soft_timer_stop(soft_timer_mgr_t *mgr, int id);

/** @thread_safe: no @isr_safe: no */
soft_timer_err_t soft_timer_reset(soft_timer_mgr_t *mgr, int id);

/** @thread_safe: no @isr_safe: yes */
bool soft_timer_is_active(const soft_timer_mgr_t *mgr, int id);

/** Call from SysTick or periodic ISR. @thread_safe: no @isr_safe: yes */
void soft_timer_tick(soft_timer_mgr_t *mgr);

/** @thread_safe: no @isr_safe: yes */
uint32_t soft_timer_get_tick(const soft_timer_mgr_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* SOFT_TIMER_H */
