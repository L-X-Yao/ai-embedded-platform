/*
 * log_rtt — RTT-style output sink for log wheel via HAL injection.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_rtt.h"

void log_rtt_init(log_rtt_t *r, log_rtt_write_fn write, void *ctx,
                  unsigned channel) {
    if (!r) return;
    r->write = write;
    r->ctx = ctx;
    r->channel = channel;
}

void log_rtt_sink(void *ctx, const char *line, size_t len) {
    log_rtt_t *r = (log_rtt_t *)ctx;
    if (!r || !r->write || !line || len == 0) return;
    r->write(r->ctx, r->channel, line, (unsigned)len);
}
