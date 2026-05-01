/*
 * scheduler -- cooperative task scheduler for bare-metal / single-threaded
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    none
 *
 * Round-robin periodic task execution, tick-driven. Application calls
 * scheduler_tick() + scheduler_run() in main loop. Passive: no OS,
 * no malloc, no timers, no interrupts.
 *
 * Thread safety: caller owns the scheduler_t struct.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Error codes
 * ====================================================================== */

typedef enum {
    SCHED_OK         =  0,
    SCHED_ERR_PARAMS = -1,
    SCHED_ERR_FULL   = -2,
} sched_err_t;

/* ======================================================================
 * Task callback
 * ====================================================================== */

typedef void (*sched_task_fn)(void *ctx);

/* ======================================================================
 * Configuration
 * ====================================================================== */

#ifndef SCHED_MAX_TASKS
#define SCHED_MAX_TASKS 16
#endif

/* ======================================================================
 * Data structures
 * ====================================================================== */

typedef struct {
    const char    *name;
    sched_task_fn  fn;
    void          *ctx;
    uint32_t       interval_ticks;
    uint32_t       last_run;
    bool           enabled;
    uint32_t       run_count;
    uint32_t       overrun_count;
} sched_task_t;

typedef struct {
    sched_task_t tasks[SCHED_MAX_TASKS];
    uint8_t      count;
    uint32_t     tick;
} scheduler_t;

/* ======================================================================
 * API
 * ====================================================================== */

/** Initialize scheduler. Must be called before any other function.
 *  @thread_safe: no */
sched_err_t scheduler_init(scheduler_t *s);

/** Register a periodic task. @thread_safe: no */
sched_err_t scheduler_add(scheduler_t *s, const char *name,
                          sched_task_fn fn, void *ctx,
                          uint32_t interval_ticks);

/** Advance the internal tick counter by one. @thread_safe: no */
void scheduler_tick(scheduler_t *s);

/** Check all tasks and run those whose interval has elapsed.
 *  @thread_safe: no */
void scheduler_run(scheduler_t *s);

/** Enable a task by name. @thread_safe: no */
sched_err_t scheduler_enable(scheduler_t *s, const char *name);

/** Disable a task by name (skipped during run). @thread_safe: no */
sched_err_t scheduler_disable(scheduler_t *s, const char *name);

/** Remove a task by name. @thread_safe: no */
sched_err_t scheduler_remove(scheduler_t *s, const char *name);

/** Return the number of registered tasks. @thread_safe: no */
uint8_t scheduler_count(const scheduler_t *s);

/** Return the run count of a named task, or 0 if not found.
 *  @thread_safe: no */
uint32_t scheduler_get_run_count(const scheduler_t *s, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
