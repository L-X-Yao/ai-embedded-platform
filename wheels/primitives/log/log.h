// SPDX-License-Identifier: MIT
/*
 * log — leveled + module-tagged logging, pure C99.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      none
 * Instances: single
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

/* #if reads an enum constant as 0, which would make every LOG_LEVEL_MAX test
 * below true, so C also gets the levels as numbers. An enum constant is an
 * int in C, so nothing else changes. C++ cannot pass an int as a log_level_t:
 * it keeps the enum constants, and LOG_LEVEL_MAX removes nothing there. */
#ifndef __cplusplus
#   define LOG_LEVEL_NONE   0
#   define LOG_LEVEL_ERROR  1
#   define LOG_LEVEL_WARN   2
#   define LOG_LEVEL_INFO   3
#   define LOG_LEVEL_DEBUG  4
#endif

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
 * @thread_safe: yes on 32-bit MCUs (single word write). On 8-bit MCUs @isr_safe: yes
 *               callers should disable interrupts around set/get.
 * ====================================================================== */

/** Set the runtime log level. @thread_safe: yes (single word write) @isr_safe: yes */
void log_set_level(log_level_t level);
/** Return the current runtime log level. @thread_safe: yes @isr_safe: yes */
log_level_t log_get_level(void);

/* ======================================================================
 * Sinks
 *
 * A sink is a byte-destination for formatted log lines.
 * log_emit() calls each registered sink in order.
 *
 * @thread_safe: log_add_sink / log_remove_sink are NOT thread-safe. @isr_safe: yes
 *               Call them during init, not from concurrent contexts.
 * ====================================================================== */

typedef void (*log_sink_fn)(void *ctx, const char *line, size_t len);

typedef enum {
    LOG_OK             =  0,
    LOG_ERR_PARAM      = -1, /* NULL function pointer */
    LOG_ERR_NO_SLOTS   = -2, /* sink table full */
    LOG_ERR_INVALID_ID = -3, /* sink_id out of range or unused */
} log_err_t;

/** Register a sink; return sink id (>=0), LOG_ERR_PARAM, or LOG_ERR_NO_SLOTS. @thread_safe: no @isr_safe: yes */
int log_add_sink(log_sink_fn fn, void *ctx);

/** Remove a sink by id; return LOG_OK or LOG_ERR_INVALID_ID. @thread_safe: no @isr_safe: yes */
int log_remove_sink(int sink_id);

/** Add preset stdout sink for host testing; returns sink id or LOG_ERR_NO_SLOTS. @thread_safe: no @isr_safe: yes */
int log_add_stdout_sink(void);

/* ======================================================================
 * Time source
 *
 * Default: disabled (no timestamps). Call log_set_time_fn() with a
 * function returning uint32_t ms to enable timestamps. Pass NULL to
 * disable again.
 * ====================================================================== */

/** Set timestamp source; pass NULL to disable timestamps. @thread_safe: no @isr_safe: yes */
void log_set_time_fn(uint32_t (*fn)(void));

/* ======================================================================
 * Main emit function. Normally called via LOG_* macros.
 *
 * Output format: "[TIMESTAMP] [L] [module] <formatted message>\n"
 *   - TIMESTAMP omitted if time_fn == NULL
 *   - L is one char: E/W/I/D
 *   - module is whatever LOG_MODULE expands to
 *
 * Formatting uses a stack buffer and needs no lock, but log_emit takes no
 * lock around the sinks either. With more than one logging context, every
 * registered sink must be reentrant (it may take its own lock), or callers
 * must serialise log_emit. log_ringbuf, log_dedup, log_rate and log_flash
 * are not reentrant. Sinks must not be added or removed after init.
 *
 * Stack: log_emit holds a LOG_LINE_BUFSIZE buffer while the formatter and then
 * each sink run on top of it, so one call needs the buffer and the deeper of
 * the two. Measured in bytes with -Os on a Cortex-M4, transport not counted:
 *
 *   formatter     no-op sink, %d %s   no-op sink, %.2f   log_color, %d %s
 *   newlib        816                 1008               856
 *   newlib-nano   720                 1024               856
 *   printf_tiny   496                 no float           856
 *
 * Size every task that logs for this plus its own frames, an interrupt frame
 * and the transport: a 1 KB task has little left. To cut the formatter, build
 * log.c with LOG_USE_PRINTF_TINY=1 and link printf_tiny.c. That helps only
 * while no sink is deeper, as log_color shows, and the first conversion
 * printf_tiny lacks ends formatting: see printf_tiny.h.
 *
 * @thread_safe: caller_must_lock (unless every sink is reentrant, see above)
 * @isr_safe: no (vsnprintf, and a LOG_LINE_BUFSIZE stack buffer)
 * ====================================================================== */

/** Format and dispatch a log line to all sinks. @thread_safe: caller_must_lock @isr_safe: no */
void log_emit(log_level_t level, const char *module, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
