// SPDX-License-Identifier: MIT
/*
 * log_dedup — deduplication sink for log wheel.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      none
 * Instances: single
 */

#include "log_dedup.h"
#include "contract.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>

void log_dedup_init(log_dedup_t *d, log_dedup_output_fn inner_fn, void *inner_ctx) {
    if (!d) return;
    d->inner_fn = inner_fn;
    d->inner_ctx = inner_ctx;
    d->last_len = 0;
    d->last_full_len = 0;
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

/* Length of a leading "[12345678] " timestamp, 0 if there is none. Only an
 * all-digit bracket counts: without a time source the first bracket is the
 * level, which is part of the message. */
static size_t timestamp_len(const char *s, size_t len) {
    size_t i = 1;

    if (len <= 4 || s[0] != '[') return 0;
    while (i < len && s[i] >= '0' && s[i] <= '9') i++;
    if (i > 1 && i + 1 < len && s[i] == ']' && s[i + 1] == ' ') return i + 2;
    return 0;
}

static int lines_equal(const char *a, size_t a_len, const char *b, size_t b_len) {
    /* Compare ignoring the leading timestamp, if present */
    size_t skip_a = timestamp_len(a, a_len);
    size_t skip_b = timestamp_len(b, b_len);
    size_t la = a_len - skip_a, lb = b_len - skip_b;

    return (la == lb) && (la > 0) && (memcmp(a + skip_a, b + skip_b, la) == 0);
}

void log_dedup_sink(void *ctx, const char *line, size_t len) {
    log_dedup_t *d = (log_dedup_t *)ctx;
    if (!d || !d->inner_fn || !line || len == 0) return;

    size_t cmp_len = len < LOG_DEDUP_LINE_MAX - 1 ? len : LOG_DEDUP_LINE_MAX - 1;
    if (d->last_len > 0 && d->last_full_len == len
        && lines_equal(d->last_line, d->last_len, line, cmp_len)) {
        if (d->repeat_count < UINT32_MAX) d->repeat_count++;
        if (d->total_suppressed < UINT32_MAX) d->total_suppressed++;
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

void log_dedup_deinit(log_dedup_t *d) {
    if (!d) return;
    (void)log_dedup_flush(d);
    d->inner_fn = NULL;
    d->inner_ctx = NULL;
    d->last_len = 0;
    d->last_full_len = 0;
    d->last_line[0] = '\0';
}

uint32_t log_dedup_suppressed(const log_dedup_t *d) {
    return d ? d->total_suppressed : 0;
}
