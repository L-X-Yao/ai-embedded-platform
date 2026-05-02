// SPDX-License-Identifier: MIT
/*
 * log_filter — module-based runtime filter sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#ifndef LOG_FILTER_H
#define LOG_FILTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG_FILTER_MAX_MODULES
#define LOG_FILTER_MAX_MODULES  8
#endif

typedef void (*log_filter_output_fn)(void *ctx, const char *line, size_t len);

typedef enum {
    LOG_FILTER_ALLOW,
    LOG_FILTER_DENY,
} log_filter_mode_t;

typedef struct {
    log_filter_output_fn inner_fn;
    void                *inner_ctx;
    log_filter_mode_t    mode;
    const char          *modules[LOG_FILTER_MAX_MODULES];
    int                  module_count;
} log_filter_t;

/* Initialize filter. Mode determines behavior:
 *   ALLOW: only forward lines matching a listed module
 *   DENY:  forward all lines EXCEPT those matching a listed module
 * Lines with no parseable module tag are always forwarded.
 * @thread_safe: no — call during init */
void log_filter_init(log_filter_t *f, log_filter_output_fn inner_fn,
                     void *inner_ctx, log_filter_mode_t mode);

/* Add a module name to the filter list. Returns 0 on success, -1 if full.
 * The pointer must remain valid (use string literals or static storage).
 * @thread_safe: no — call during init */
int log_filter_add(log_filter_t *f, const char *module);

/* Clear all module entries.
 * @thread_safe: no */
void log_filter_clear(log_filter_t *f);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_filter_t* as ctx when registering via log_add_sink().
 * @thread_safe: yes if module list is not modified concurrently */
void log_filter_sink(void *ctx, const char *line, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LOG_FILTER_H */
