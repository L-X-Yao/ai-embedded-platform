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
 *
 * Thread safety: tick runs in one context, usually a periodic ISR. One other
 * context may use the rest of the API beside it with no lock.
 *
 *   tick side   soft_timer_tick, and whatever a callback calls
 *   other side  soft_timer_create, destroy, start, stop, reset, is_active,
 *               get_tick
 *   neither     soft_timer_init, which must finish before tick starts
 *
 * The tick must be able to interrupt the other side, and not the reverse. If
 * tick runs in the main loop and an ISR calls start, stop or reset, mask that
 * interrupt around tick: otherwise a stop can be followed by one more callback
 * and a reset can be lost.
 *
 * This holds where a 32-bit aligned load or store is a single access, which
 * is every Cortex-M. On an 8-bit or 16-bit MCU the fields can tear, so mask
 * the tick's interrupt around the calls there.
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

/*
 * Keeps a timer's fields on the right side of its active flag. A compiler
 * barrier is enough on a single core, because a core sees its own accesses in
 * program order and an interrupt does not change that. If tick runs on another
 * core, define this as a real fence, such as __DMB(), before including this
 * header or on the command line.
 */
#ifndef SOFT_TIMER_BARRIER
#if defined(__GNUC__)
#define SOFT_TIMER_BARRIER() __asm__ volatile("" ::: "memory")
#else
#error "define SOFT_TIMER_BARRIER() as this compiler's memory barrier"
#endif
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

/* What a tick can change is volatile. active is uint8_t, not bool, because
 * GCC 13's -fsanitize=bool reads a volatile bool as a plain byte, and a poll
 * then never sees the change. The other fields change only while the timer is
 * inactive. */
typedef struct {
    uint32_t          period;
    volatile uint32_t remaining;
    soft_timer_cb_t   callback;
    void             *user_ctx;
    soft_timer_mode_t mode;
    volatile uint8_t  active;
} soft_timer_entry_t;

typedef struct {
    soft_timer_entry_t timers[SOFT_TIMER_MAX];
    volatile size_t    count;
    volatile uint32_t  tick_count;
} soft_timer_mgr_t;

/** Finish before tick starts. @thread_safe: no @isr_safe: no */
soft_timer_err_t soft_timer_init(soft_timer_mgr_t *mgr);

/** Other side only: safe beside a running tick, not from a callback in an ISR.
 *  @thread_safe: no @isr_safe: no */
int soft_timer_create(soft_timer_mgr_t *mgr, uint32_t period_ticks,
                      soft_timer_mode_t mode, soft_timer_cb_t cb,
                      void *user_ctx);

/** Release a timer slot for reuse. Other side only: safe beside a running
 *  tick, not from a callback in an ISR. @thread_safe: no @isr_safe: no */
soft_timer_err_t soft_timer_destroy(soft_timer_mgr_t *mgr, int id);

/** Start or resume.  A stopped timer keeps what was left of its period; a
 *  fired one-shot gets a full period, even if it fires during this call.
 *  @thread_safe: safe beside tick @isr_safe: yes */
soft_timer_err_t soft_timer_start(soft_timer_mgr_t *mgr, int id);

/** No callback runs for this timer once this returns.
 *  @thread_safe: safe beside tick @isr_safe: yes */
soft_timer_err_t soft_timer_stop(soft_timer_mgr_t *mgr, int id);

/** @thread_safe: safe beside tick @isr_safe: yes */
soft_timer_err_t soft_timer_reset(soft_timer_mgr_t *mgr, int id);

/** Safe to poll in a loop. @thread_safe: yes (read only) @isr_safe: yes */
bool soft_timer_is_active(const soft_timer_mgr_t *mgr, int id);

/** Call from SysTick or periodic ISR, from one context only.
 *  @thread_safe: no @isr_safe: yes */
void soft_timer_tick(soft_timer_mgr_t *mgr);

/** Safe to poll in a loop. @thread_safe: yes (read only) @isr_safe: yes */
uint32_t soft_timer_get_tick(const soft_timer_mgr_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* SOFT_TIMER_H */
