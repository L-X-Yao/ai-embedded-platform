/*
 * log — leveled + module-tagged logging, pure C99.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */
#ifndef LOG_H
#define LOG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Levels
 * ====================================================================== */

typedef enum {
    LOG_LEVEL_NONE  = 0,  /* disable marker; not a real log level */
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_INFO  = 3,
    LOG_LEVEL_DEBUG = 4,
} log_level_t;

/* Compile-time maximum level. Higher levels compile out completely.
 * Set to LOG_LEVEL_WARN in release build to drop DEBUG/INFO strings.
 * Override via -DLOG_LEVEL_MAX=... at build time. */
#ifndef LOG_LEVEL_MAX
#   define LOG_LEVEL_MAX  LOG_LEVEL_DEBUG
#endif

/* ======================================================================
 * Main macros (preferred usage)
 *
 * Every .c file that logs must define LOG_MODULE before including log.h:
 *     #define LOG_MODULE "mymodule"
 *     #include "log.h"
 *
 * LOG_LEVEL_MAX filters at compile time (emitted strings are optimized out).
 * log_set_level() filters at runtime (strings still in binary).
 * ====================================================================== */

/* True compile-time elimination: levels above LOG_LEVEL_MAX expand to
 * ((void)0), so both the log_emit() call AND argument evaluation (side
 * effects!) are removed by preprocessor — no dependency on LTO. */
#if LOG_LEVEL_MAX >= LOG_LEVEL_ERROR
#   define LOG_ERROR(fmt, ...)  log_emit(LOG_LEVEL_ERROR, LOG_MODULE, (fmt), ##__VA_ARGS__)
#else
#   define LOG_ERROR(fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL_MAX >= LOG_LEVEL_WARN
#   define LOG_WARN(fmt, ...)   log_emit(LOG_LEVEL_WARN,  LOG_MODULE, (fmt), ##__VA_ARGS__)
#else
#   define LOG_WARN(fmt, ...)   ((void)0)
#endif

#if LOG_LEVEL_MAX >= LOG_LEVEL_INFO
#   define LOG_INFO(fmt, ...)   log_emit(LOG_LEVEL_INFO,  LOG_MODULE, (fmt), ##__VA_ARGS__)
#else
#   define LOG_INFO(fmt, ...)   ((void)0)
#endif

#if LOG_LEVEL_MAX >= LOG_LEVEL_DEBUG
#   define LOG_DEBUG(fmt, ...)  log_emit(LOG_LEVEL_DEBUG, LOG_MODULE, (fmt), ##__VA_ARGS__)
#else
#   define LOG_DEBUG(fmt, ...)  ((void)0)
#endif

/* ======================================================================
 * Runtime level control
 * @thread_safe: yes (atomic word write)
 * ====================================================================== */

void log_set_level(log_level_t level);
log_level_t log_get_level(void);

/* ======================================================================
 * Sinks
 *
 * A sink is a byte-destination for formatted log lines.
 * log_emit() calls each registered sink in order.
 *
 * @thread_safe: log_add_sink / log_remove_sink are NOT thread-safe.
 *               Call them during init, not from concurrent contexts.
 * ====================================================================== */

typedef void (*log_sink_fn)(void *ctx, const char *line, size_t len);

/* Register a sink. Returns sink id (>=0) or -1 on no slots. */
int log_add_sink(log_sink_fn fn, void *ctx);

/* Returns 0 on success, -1 on invalid id. */
int log_remove_sink(int sink_id);

/* ======================================================================
 * Time source
 *
 * Default: disabled (no timestamps). Call log_set_time_fn() with a
 * function returning uint32_t ms to enable timestamps. Pass NULL to
 * disable again.
 * ====================================================================== */

void log_set_time_fn(uint32_t (*fn)(void));

/* ======================================================================
 * Main emit function. Normally called via LOG_* macros.
 *
 * Output format: "[TIMESTAMP] [L] [module] <formatted message>\n"
 *   - TIMESTAMP omitted if time_fn == NULL
 *   - L is one char: E/W/I/D
 *   - module is whatever LOG_MODULE expands to
 *
 * @thread_safe: yes (uses stack buffer, no shared state during format)
 * ====================================================================== */

void log_emit(log_level_t level, const char *module, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
