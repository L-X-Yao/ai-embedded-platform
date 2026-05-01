/*
 * log_dedup — deduplication sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#ifndef LOG_DEDUP_H
#define LOG_DEDUP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG_DEDUP_LINE_MAX
#define LOG_DEDUP_LINE_MAX  256
#endif

typedef void (*log_dedup_output_fn)(void *ctx, const char *line, size_t len);

typedef struct {
    log_dedup_output_fn inner_fn;
    void               *inner_ctx;
    char                last_line[LOG_DEDUP_LINE_MAX];
    size_t              last_len;
    size_t              last_full_len;
    uint32_t            repeat_count;
    uint32_t            total_suppressed;
} log_dedup_t;

/* @thread_safe: no — call during init */
void log_dedup_init(log_dedup_t *d, log_dedup_output_fn inner_fn, void *inner_ctx);

/* Sink adapter: compatible with log_sink_fn.
 * Suppresses consecutive identical lines. When a new different line
 * arrives after N repeats, emits "[repeated N times]" before the new line.
 * @thread_safe: no (modifies last_line state) */
void log_dedup_sink(void *ctx, const char *line, size_t len);

/* Flush pending repeat count (call before shutdown).
 * @thread_safe: no */
void log_dedup_flush(log_dedup_t *d);

/* Teardown: flush pending repeats and clear state.
 * @thread_safe: no */
void log_dedup_deinit(log_dedup_t *d);

/* Return total lines suppressed since init.
 * @thread_safe: no */
uint32_t log_dedup_suppressed(const log_dedup_t *d);

#ifdef __cplusplus
}
#endif

#endif /* LOG_DEDUP_H */
