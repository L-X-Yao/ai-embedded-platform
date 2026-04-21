/*
 * log — leveled + module-tagged logging, wraps vsf_trace.
 *
 * Layer:   L0
 * Status:  draft
 * Deps:    VSF kernel, vsf_trace (optional sink), vsf_stream (optional sink)
 */
#ifndef LOG_H
#define LOG_H

#include "vsf.h"
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

#define LOG_ERROR(fmt, ...)  LOG_AT_(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   LOG_AT_(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   LOG_AT_(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)  LOG_AT_(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

/* Internal; not for direct use */
#define LOG_AT_(level, fmt, ...)                                               \
    do {                                                                       \
        if ((int)(level) <= (int)LOG_LEVEL_MAX) {                              \
            log_emit((level), LOG_MODULE, (fmt), ##__VA_ARGS__);               \
        }                                                                      \
    } while (0)

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

/* Preset sink: route to vsf_trace's internal stream.
 * Requires vsf_trace to have been initialized (vsf_start_trace + stream).
 * Returns sink id. */
int log_add_vsf_trace_sink(void);

/* Preset sink: route to an arbitrary VSF stream. */
int log_add_stream_sink(vsf_stream_t *stream);

/* ======================================================================
 * Time source
 *
 * Default: vsf_systimer-based ms. Override by calling log_set_time_fn()
 * with a function returning uint32_t ms (or NULL to disable timestamps).
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
