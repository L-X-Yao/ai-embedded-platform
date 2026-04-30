/*
 * log_rate — rate-limiting sink for log wheel (token bucket).
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_rate.h"

void log_rate_init(log_rate_t *r, log_rate_output_fn inner_fn, void *inner_ctx,
                   log_rate_time_fn time_fn,
                   uint32_t max_per_interval, uint32_t interval_ms) {
    if (!r) return;
    r->inner_fn = inner_fn;
    r->inner_ctx = inner_ctx;
    r->time_fn = time_fn;
    r->max_tokens = max_per_interval;
    r->refill_interval_ms = interval_ms > 0 ? interval_ms : 1;
    r->tokens = max_per_interval;
    r->last_refill_ms = time_fn ? time_fn() : 0;
    r->dropped = 0;
}

static void refill(log_rate_t *r, uint32_t now) {
    uint32_t elapsed = now - r->last_refill_ms;
    if (elapsed >= r->refill_interval_ms) {
        r->tokens = r->max_tokens;
        r->last_refill_ms = now - (elapsed % r->refill_interval_ms);
    }
}

void log_rate_sink(void *ctx, const char *line, size_t len) {
    log_rate_t *r = (log_rate_t *)ctx;
    if (!r || !r->inner_fn || !line || len == 0) return;

    if (r->time_fn) {
        refill(r, r->time_fn());
    }

    if (r->tokens > 0) {
        r->tokens--;
        r->inner_fn(r->inner_ctx, line, len);
    } else {
        r->dropped++;
    }
}

uint32_t log_rate_dropped(const log_rate_t *r) {
    return r ? r->dropped : 0;
}

void log_rate_reset(log_rate_t *r) {
    if (!r) return;
    r->tokens = r->max_tokens;
    r->dropped = 0;
    if (r->time_fn) r->last_refill_ms = r->time_fn();
}
