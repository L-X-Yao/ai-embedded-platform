/*
 * log — leveled + module-tagged logging, pure C99.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ======================================================================
 * Config
 * ====================================================================== */

#ifndef LOG_MAX_SINKS
#   define LOG_MAX_SINKS       4
#endif

#ifndef LOG_LINE_BUFSIZE
#   define LOG_LINE_BUFSIZE    256
#endif

/* ======================================================================
 * State
 * ====================================================================== */

typedef struct {
    log_sink_fn  fn;
    void        *ctx;
    int          in_use;
} log_sink_slot_t;

static volatile log_level_t g_runtime_level = LOG_LEVEL_INFO;
static log_sink_slot_t  g_sinks[LOG_MAX_SINKS];

static uint32_t (* volatile g_time_fn)(void) = NULL;

/* Indexed by log_level_t value (0..4). */
static const char LOG_LEVEL_CHAR[] = { 'N', 'E', 'W', 'I', 'D' };

/* ======================================================================
 * Runtime level
 * ====================================================================== */

void log_set_level(log_level_t level) {
    g_runtime_level = level;
}

log_level_t log_get_level(void) {
    return g_runtime_level;
}

/* ======================================================================
 * Time source
 * ====================================================================== */

void log_set_time_fn(uint32_t (*fn)(void)) {
    g_time_fn = fn;
}

/* ======================================================================
 * Sinks
 * ====================================================================== */

int log_add_sink(log_sink_fn fn, void *ctx) {
    if (!fn) return -1;
    for (int i = 0; i < LOG_MAX_SINKS; i++) {
        if (!g_sinks[i].in_use) {
            g_sinks[i].fn = fn;
            g_sinks[i].ctx = ctx;
            g_sinks[i].in_use = 1;
            return i;
        }
    }
    return -1;  /* no slots */
}

int log_remove_sink(int sink_id) {
    if (sink_id < 0 || sink_id >= LOG_MAX_SINKS || !g_sinks[sink_id].in_use) {
        return -1;
    }
    g_sinks[sink_id].fn = NULL;
    g_sinks[sink_id].ctx = NULL;
    g_sinks[sink_id].in_use = 0;
    return 0;
}

/* ======================================================================
 * Emit
 * ====================================================================== */

void log_emit(log_level_t level, const char *module, const char *fmt, ...) {
    /* runtime filter (compile-time is in the macros) */
    if (level == LOG_LEVEL_NONE || level > g_runtime_level) {
        return;
    }

    /* per-call stack buffer => thread-safe without locks */
    char buf[LOG_LINE_BUFSIZE];
    int pos = 0;
    int rem = (int)sizeof(buf);
    int n;

    /* Snapshot volatile pointer once to avoid TOCTOU with log_set_time_fn() */
    uint32_t (*time_fn)(void) = g_time_fn;
    if (time_fn) {
        uint32_t ts = time_fn();
        n = snprintf(buf + pos, (size_t)rem, "[%08u] ", (unsigned)ts);
        if (n > 0 && n < rem) { pos += n; rem -= n; }
    }

    /* Level + module */
    char lvl_char = ((int)level >= 0 && (int)level < (int)sizeof(LOG_LEVEL_CHAR))
                    ? LOG_LEVEL_CHAR[(int)level] : '?';
    n = snprintf(buf + pos, (size_t)rem, "[%c] [%s] ",
                 lvl_char, module ? module : "-");
    if (n > 0 && n < rem) { pos += n; rem -= n; }

    /* User message */
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buf + pos, (size_t)rem, fmt, ap);
    va_end(ap);
    if (n > 0) {
        if (n >= rem) {
            pos += rem - 1;   /* truncated */
        } else {
            pos += n;
        }
    }

    /* Append newline (ensure at least 1 byte free) */
    if (pos >= (int)sizeof(buf)) pos = (int)sizeof(buf) - 1;
    buf[pos++] = '\n';
    if (pos < (int)sizeof(buf)) buf[pos] = '\0';  /* convenience for sinks */

    /* Dispatch */
    for (int i = 0; i < LOG_MAX_SINKS; i++) {
        if (g_sinks[i].in_use && g_sinks[i].fn) {
            g_sinks[i].fn(g_sinks[i].ctx, buf, (size_t)pos);
        }
    }
}
