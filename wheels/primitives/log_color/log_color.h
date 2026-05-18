// SPDX-License-Identifier: MIT
/*
 * log_color — ANSI color wrapper sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      none
 * Instances: multi
 */

#ifndef LOG_COLOR_H
#define LOG_COLOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*log_color_output_fn)(void *ctx, const char *line, size_t len);

typedef struct {
    log_color_output_fn inner_fn;
    void               *inner_ctx;
} log_color_t;

/* @thread_safe: no — call during init @isr_safe: no */
void log_color_init(log_color_t *lc, log_color_output_fn inner_fn, void *inner_ctx);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_color_t* as ctx when registering via log_add_sink().
 * Parses level char from log line, wraps with ANSI color, forwards to inner.
 * @thread_safe: yes (no shared state modified) @isr_safe: yes */
void log_color_sink(void *ctx, const char *line, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LOG_COLOR_H */
