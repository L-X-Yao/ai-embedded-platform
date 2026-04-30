/*
 * log_ringbuf — RAM circular log buffer with line-oriented retrieval.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#include "log_ringbuf.h"
#include <string.h>

void log_ringbuf_init(log_ringbuf_t *rb, uint8_t *buf, size_t capacity) {
    if (!rb) return;
    rb->buf = buf;
    rb->capacity = (buf && capacity > 0) ? capacity : 0;
    rb->head = 0;
    rb->count = 0;
    rb->total_lines = 0;
    rb->dropped_lines = 0;
}

void log_ringbuf_write(log_ringbuf_t *rb, const char *line, size_t len) {
    if (!rb || !rb->buf || rb->capacity == 0 || !line || len == 0) return;

    rb->total_lines++;

    if (len > rb->capacity) {
        rb->dropped_lines++;
        return;
    }

    /* Evict oldest bytes until enough space */
    while (rb->count + len > rb->capacity) {
        rb->head = (rb->head + 1) % rb->capacity;
        rb->count--;
        rb->dropped_lines++;
    }

    /* Write at tail */
    size_t tail = (rb->head + rb->count) % rb->capacity;
    size_t first = rb->capacity - tail;
    if (first >= len) {
        memcpy(rb->buf + tail, line, len);
    } else {
        memcpy(rb->buf + tail, line, first);
        memcpy(rb->buf, line + first, len - first);
    }
    rb->count += len;
}

void log_ringbuf_sink(void *ctx, const char *line, size_t len) {
    log_ringbuf_write((log_ringbuf_t *)ctx, line, len);
}

size_t log_ringbuf_read(const log_ringbuf_t *rb, char *dst, size_t dst_size) {
    if (!rb || !rb->buf || !dst || dst_size == 0) return 0;

    size_t to_copy = rb->count < dst_size ? rb->count : dst_size;
    size_t first = rb->capacity - rb->head;
    if (first >= to_copy) {
        memcpy(dst, rb->buf + rb->head, to_copy);
    } else {
        memcpy(dst, rb->buf + rb->head, first);
        memcpy(dst + first, rb->buf, to_copy - first);
    }
    return to_copy;
}

size_t log_ringbuf_used(const log_ringbuf_t *rb) {
    return rb ? rb->count : 0;
}

uint32_t log_ringbuf_total(const log_ringbuf_t *rb) {
    return rb ? rb->total_lines : 0;
}

uint32_t log_ringbuf_dropped(const log_ringbuf_t *rb) {
    return rb ? rb->dropped_lines : 0;
}

void log_ringbuf_clear(log_ringbuf_t *rb) {
    if (!rb) return;
    rb->head = 0;
    rb->count = 0;
}
