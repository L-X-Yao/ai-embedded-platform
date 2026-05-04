/*
 * runtime_monitor.h -- production contracts that degrade instead of aborting
 *
 * Unlike contract.h (which aborts on violation), these:
 * - Log the violation via a configurable callback
 * - Return an error code instead of aborting
 * - Track violation count for telemetry
 *
 * Enable with -DRUNTIME_MONITOR_ENABLED
 *
 * Usage:
 *   #include "runtime_monitor.h"
 *
 *   int sensor_read(sensor_t *s, float *out) {
 *       MONITOR_REQUIRE(s != NULL, -1);
 *       MONITOR_REQUIRE(s->initialized, -1);
 *       // ... read hardware ...
 *       MONITOR_ENSURE(result >= SENSOR_MIN && result <= SENSOR_MAX);
 *       return 0;
 *   }
 *
 * Telemetry in production:
 *   runtime_monitor_set_callback(my_telemetry_handler);
 *   // ... run application ...
 *   uint32_t total = runtime_monitor_violation_count();
 *   uint32_t warns = runtime_monitor_count_by_severity(MONITOR_WARN);
 *
 * Thread safety: violation counter uses no locking. On 32-bit MCUs the
 * counter is a single-word atomic-width type, so reads/writes are
 * tear-free. For strict multi-core atomicity, wrap the callback in
 * your platform's critical section.
 */

#ifndef RUNTIME_MONITOR_H
#define RUNTIME_MONITOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Severity levels ---------------------------------------------------- */

typedef enum {
    MONITOR_DEBUG = 0,   /* Development-only, stripped in release   */
    MONITOR_INFO  = 1,   /* Informational, no action needed        */
    MONITOR_WARN  = 2,   /* Unexpected but recoverable             */
    MONITOR_ERROR = 3,   /* Contract violation, error returned      */
    MONITOR_FATAL = 4,   /* Unrecoverable, system should restart   */
} runtime_monitor_severity_t;

/* ---- Violation info passed to callback ---------------------------------- */

typedef struct {
    const char                *file;       /* Source file (__FILE__)          */
    int                        line;       /* Source line (__LINE__)          */
    const char                *condition;  /* Stringified condition (#cond)   */
    runtime_monitor_severity_t severity;   /* Severity of this violation     */
    uint32_t                   total_count;/* Running violation count         */
} runtime_monitor_event_t;

/* ---- Callback type ------------------------------------------------------ */

/**
 * User-provided violation handler.
 *
 * Called on every violation when RUNTIME_MONITOR_ENABLED is defined.
 * Must be safe to call from any context (ISR, task, bare-metal main).
 * Keep it short -- log and return, do not block.
 *
 * Set to NULL to disable logging (counter still increments).
 */
typedef void (*runtime_monitor_callback_t)(const runtime_monitor_event_t *event);

/* ---- API (all inline, zero link dependency) ----------------------------- */

#ifdef RUNTIME_MONITOR_ENABLED

/* ---- Internal state (file-scope statics in header-only design) ---------- */

static uint32_t runtime_monitor__total_count = 0;
static uint32_t runtime_monitor__count_by_sev[5] = {0, 0, 0, 0, 0};
static runtime_monitor_callback_t runtime_monitor__callback = (runtime_monitor_callback_t)0;

/**
 * Install a violation callback. Pass NULL to disable.
 */
static inline void runtime_monitor_set_callback(runtime_monitor_callback_t cb)
{
    runtime_monitor__callback = cb;
}

/**
 * Get total violation count since startup / last reset.
 */
static inline uint32_t runtime_monitor_violation_count(void)
{
    return runtime_monitor__total_count;
}

/**
 * Get violation count for a specific severity level.
 */
static inline uint32_t runtime_monitor_count_by_severity(
    runtime_monitor_severity_t sev)
{
    if ((unsigned)sev > 4u) { return 0; }
    return runtime_monitor__count_by_sev[(unsigned)sev];
}

/**
 * Reset all counters to zero.
 */
static inline void runtime_monitor_reset_counts(void)
{
    runtime_monitor__total_count = 0;
    for (int i = 0; i < 5; i++) {
        runtime_monitor__count_by_sev[i] = 0;
    }
}

/**
 * Core reporting function. Called by macros -- not intended for direct use.
 */
static inline void runtime_monitor__report(
    const char *file, int line, const char *cond,
    runtime_monitor_severity_t sev)
{
    runtime_monitor__total_count++;
    if ((unsigned)sev <= 4u) {
        runtime_monitor__count_by_sev[(unsigned)sev]++;
    }

    if (runtime_monitor__callback) {
        runtime_monitor_event_t ev;
        ev.file        = file;
        ev.line        = line;
        ev.condition   = cond;
        ev.severity    = sev;
        ev.total_count = runtime_monitor__total_count;
        runtime_monitor__callback(&ev);
    }
}

/* ---- Contract macros (enabled) ------------------------------------------ */

/**
 * MONITOR_REQUIRE -- precondition check that returns err_ret on failure.
 *
 * Use at function entry to validate arguments / state.
 * On violation: logs at ERROR severity, returns err_ret.
 */
#define MONITOR_REQUIRE(cond, err_ret) \
    do { if (!(cond)) { \
        runtime_monitor__report(__FILE__, __LINE__, #cond, MONITOR_ERROR); \
        return (err_ret); \
    }} while(0)

/**
 * MONITOR_REQUIRE_SEV -- like MONITOR_REQUIRE with explicit severity.
 */
#define MONITOR_REQUIRE_SEV(cond, err_ret, sev) \
    do { if (!(cond)) { \
        runtime_monitor__report(__FILE__, __LINE__, #cond, (sev)); \
        return (err_ret); \
    }} while(0)

/**
 * MONITOR_ENSURE -- postcondition check (no return, just log).
 *
 * Use after computation to assert invariants. Does not alter control flow.
 * On violation: logs at WARN severity.
 */
#define MONITOR_ENSURE(cond) \
    do { if (!(cond)) { \
        runtime_monitor__report(__FILE__, __LINE__, #cond, MONITOR_WARN); \
    }} while(0)

/**
 * MONITOR_ENSURE_SEV -- like MONITOR_ENSURE with explicit severity.
 */
#define MONITOR_ENSURE_SEV(cond, sev) \
    do { if (!(cond)) { \
        runtime_monitor__report(__FILE__, __LINE__, #cond, (sev)); \
    }} while(0)

/**
 * MONITOR_CHECK -- general inline check, returns err_ret on failure.
 *
 * Like MONITOR_REQUIRE but for mid-function checks (e.g., hardware
 * status, return values from called functions).
 * On violation: logs at WARN severity, returns err_ret.
 */
#define MONITOR_CHECK(cond, err_ret) \
    do { if (!(cond)) { \
        runtime_monitor__report(__FILE__, __LINE__, #cond, MONITOR_WARN); \
        return (err_ret); \
    }} while(0)

/**
 * MONITOR_ASSERT_FATAL -- unrecoverable state, log and fall through.
 *
 * For truly broken invariants where returning is not enough.
 * Logs at FATAL severity. Does NOT abort (unlike contract.h).
 * Caller should handle (e.g., trigger watchdog reset).
 */
#define MONITOR_ASSERT_FATAL(cond) \
    do { if (!(cond)) { \
        runtime_monitor__report(__FILE__, __LINE__, #cond, MONITOR_FATAL); \
    }} while(0)

#else /* RUNTIME_MONITOR_ENABLED not defined -- all macros compile away */

#define MONITOR_REQUIRE(cond, err_ret)          ((void)0)
#define MONITOR_REQUIRE_SEV(cond, err_ret, sev) ((void)0)
#define MONITOR_ENSURE(cond)                    ((void)0)
#define MONITOR_ENSURE_SEV(cond, sev)           ((void)0)
#define MONITOR_CHECK(cond, err_ret)            ((void)0)
#define MONITOR_ASSERT_FATAL(cond)              ((void)0)

static inline void runtime_monitor_set_callback(runtime_monitor_callback_t cb)
{
    (void)cb;
}

static inline uint32_t runtime_monitor_violation_count(void)
{
    return 0;
}

static inline uint32_t runtime_monitor_count_by_severity(
    runtime_monitor_severity_t sev)
{
    (void)sev;
    return 0;
}

static inline void runtime_monitor_reset_counts(void)
{
    /* no-op */
}

#endif /* RUNTIME_MONITOR_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_MONITOR_H */
