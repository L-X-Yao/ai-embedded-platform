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

    if (la > 4 && sa[0] == '[') {
        for (size_t i = 1; i < la - 1; i++) {
            if (sa[i] == ']' && sa[i + 1] == ' ') { sa += i + 2; la -= i + 2; break; }
        }
    }
    if (lb > 4 && sb[0] == '[') {
        for (size_t i = 1; i < lb - 1; i++) {
            if (sb[i] == ']' && sb[i + 1] == ' ') { sb += i + 2; lb -= i + 2; break; }
        }
    }

    return (la == lb) && (memcmp(sa, sb, la) == 0);
}

void log_dedup_sink(void *ctx, const char *line, size_t len) {
    log_dedup_t *d = (log_dedup_t *)ctx;
    if (!d || !d->inner_fn || !line || len == 0) return;

    size_t cmp_len = len < LOG_DEDUP_LINE_MAX - 1 ? len : LOG_DEDUP_LINE_MAX - 1;
    if (d->last_len > 0 && d->last_full_len == len
        && lines_equal(d->last_line, d->last_len, line, cmp_len)) {
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
    d->last_full_len = len;
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
