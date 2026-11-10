// SPDX-License-Identifier: MIT
/*
 * task_watchdog — passive tick-driven task health monitor
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Registers tasks with a timeout. Each task calls feed() periodically.
 * Application calls tick() to advance time and check() to detect
 * expired tasks. Callback fires on expiry. Fully passive — no timers,
 * no interrupts, no OS dependency.
 */

#ifndef TASK_WATCHDOG_H
#define TASK_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TASK_WD_MAX_TASKS
#define TASK_WD_MAX_TASKS  8
#endif
#if TASK_WD_MAX_TASKS > 255
#error "TASK_WD_MAX_TASKS exceeds 255, the most a uint8_t count can hold"
#endif

typedef enum {
    TASK_WD_OK            =  0,
    TASK_WD_ERR_PARAM    = -1,
    TASK_WD_ERR_FULL      = -2,
    TASK_WD_ERR_NOT_FOUND = -3,
} task_wd_err_t;

typedef enum {
    TASK_WD_STATUS_OK       = 0,
    TASK_WD_STATUS_EXPIRED,
    TASK_WD_STATUS_DISABLED,
} task_wd_status_t;

/**
 * Expiry callback. Fires from task_watchdog_check() context.
 * Do not call task_watchdog_check() from within the callback.
 */
typedef void (*task_wd_callback_t)(void *ctx, const char *task_name,
                                   uint32_t last_feed);

typedef struct {
    const char      *name;
    uint32_t         timeout;       /* max ticks between feeds         */
    uint32_t         last_feed;     /* tick count at last feed          */
    task_wd_status_t status;
    bool             enabled;
} task_wd_entry_t;

typedef struct {
    task_wd_entry_t   tasks[TASK_WD_MAX_TASKS];
    uint8_t           count;
    uint32_t          tick_count;
    task_wd_callback_t on_expired;
    void             *expired_ctx;
} task_watchdog_t;

/** @thread_safe: no */
task_wd_err_t task_watchdog_init(task_watchdog_t *wd);

/**
 * Start watching a task.  A name already registered is restarted with the new
 * timeout: enabled, status OK, and fed as of now.
 *
 * The name is kept by pointer, not copied, and is the pointer the expiry
 * callback gets. It must stay valid and unchanged for as long as wd is in use:
 * a string literal or a static array. A stack buffer gives a dead name, and
 * one buffer reused for two tasks reads as a restart of the first, so only
 * one task is watched, with no error.
 * @thread_safe: caller_must_lock
 */
task_wd_err_t task_watchdog_register(task_watchdog_t *wd, const char *name,
                                     uint32_t timeout_ticks);

/** @thread_safe: caller_must_lock */
task_wd_err_t task_watchdog_feed(task_watchdog_t *wd, const char *name);

/** Check all tasks, call callback for expired ones. @thread_safe: caller_must_lock */
void task_watchdog_check(task_watchdog_t *wd);

/** Advance internal tick counter by 1. @thread_safe: caller_must_lock */
void task_watchdog_tick(task_watchdog_t *wd);

/** @thread_safe: caller_must_lock */
void task_watchdog_set_callback(task_watchdog_t *wd, task_wd_callback_t cb,
                                void *ctx);

/** @thread_safe: caller_must_lock */
task_wd_err_t task_watchdog_enable(task_watchdog_t *wd, const char *name);

/** @thread_safe: caller_must_lock */
task_wd_err_t task_watchdog_disable(task_watchdog_t *wd, const char *name);

/** @thread_safe: caller_must_lock */
task_wd_status_t task_watchdog_status(const task_watchdog_t *wd,
                                      const char *name);

/** @thread_safe: caller_must_lock */
uint8_t task_watchdog_count(const task_watchdog_t *wd);

/** @thread_safe: caller_must_lock */
uint32_t task_watchdog_expired_count(const task_watchdog_t *wd);

#ifdef __cplusplus
}
#endif

#endif /* TASK_WATCHDOG_H */
