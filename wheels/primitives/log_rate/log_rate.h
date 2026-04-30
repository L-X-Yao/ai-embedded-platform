/*
 * log_rate — rate-limiting sink for log wheel (token bucket).
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#ifndef LOG_RATE_H
#define LOG_RATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*log_rate_output_fn)(void *ctx, const char *line, size_t len);
typedef uint32_t (*log_rate_time_fn)(void);

typedef struct {
    log_rate_output_fn inner_fn;
    void              *inner_ctx;
    log_rate_time_fn   time_fn;
    uint32_t           max_tokens;
    uint32_t           refill_interval_ms;
    uint32_t           tokens;
    uint32_t           last_refill_ms;
    uint32_t           dropped;
} log_rate_t;

/* Initialize rate limiter.
 *   max_per_interval: max lines forwarded per interval
 *   interval_ms: refill interval in milliseconds
 *   time_fn: returns current time in ms (injected, no OS dependency)
 * @thread_safe: no — call during init */
void log_rate_init(log_rate_t *r, log_rate_output_fn inner_fn, void *inner_ctx,
                   log_rate_time_fn time_fn,
                   uint32_t max_per_interval, uint32_t interval_ms);

/* Sink adapter: compatible with log_sink_fn.
 * Pass log_rate_t* as ctx when registering via log_add_sink().
 * @thread_safe: no (modifies token state) */
void log_rate_sink(void *ctx, const char *line, size_t len);

/* Return number of lines dropped since init.
 * @thread_safe: no */
uint32_t log_rate_dropped(const log_rate_t *r);

/* Reset token bucket and dropped counter.
 * @thread_safe: no */
void log_rate_reset(log_rate_t *r);

#ifdef __cplusplus
}
#endif

#endif /* LOG_RATE_H */
