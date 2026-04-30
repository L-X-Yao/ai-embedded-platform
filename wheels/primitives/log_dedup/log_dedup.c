/*
 * log_dedup — deduplication sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_dedup.h"
#include <string.h>
#include <stdio.h>

void log_dedup_init(log_dedup_t *d, log_dedup_output_fn inner_fn, void *inner_ctx) {
    if (!d) return;
    d->inner_fn = inner_fn;
    d->inner_ctx = inner_ctx;
    d->last_len = 0;
    d->last_line[0] = '\0';
    d->repeat_count = 0;
    d->total_suppressed = 0;
}

static void emit_repeat(log_dedup_t *d) {
    if (d->repeat_count > 0 && d->inner_fn) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf),
                         "  [repeated %u time%s]\n",
                         (unsigned)d->repeat_count,
                         d->repeat_count == 1 ? "" : "s");
        if (n > 0) {
            d->inner_fn(d->inner_ctx, buf, (size_t)n);
        }
    }
}

static int lines_equal(const char *a, size_t a_len, const char *b, size_t b_len) {
    /* Compare ignoring leading timestamp: skip "[XXXXXXXX] " prefix if present */
    const char *sa = a, *sb = b;
    size_t la = a_len, lb = b_len;

    if (la > 12 && sa[0] == '[' && sa[9] == ']' && sa[10] == ' ') {
        sa += 11; la -= 11;
    }
    if (lb > 12 && sb[0] == '[' && sb[9] == ']' && sb[10] == ' ') {
        sb += 11; lb -= 11;
    }

    return (la == lb) && (memcmp(sa, sb, la) == 0);
}

void log_dedup_sink(void *ctx, const char *line, size_t len) {
    log_dedup_t *d = (log_dedup_t *)ctx;
    if (!d || !d->inner_fn || !line || len == 0) return;

    if (d->last_len > 0 && lines_equal(d->last_line, d->last_len, line, len)) {
        d->repeat_count++;
        d->total_suppressed++;
        return;
    }

    emit_repeat(d);

    d->inner_fn(d->inner_ctx, line, len);

    size_t store = len < LOG_DEDUP_LINE_MAX - 1 ? len : LOG_DEDUP_LINE_MAX - 1;
    memcpy(d->last_line, line, store);
    d->last_line[store] = '\0';
    d->last_len = store;
    d->repeat_count = 0;
}

void log_dedup_flush(log_dedup_t *d) {
    if (!d) return;
    emit_repeat(d);
    d->repeat_count = 0;
}

uint32_t log_dedup_suppressed(const log_dedup_t *d) {
    return d ? d->total_suppressed : 0;
}
