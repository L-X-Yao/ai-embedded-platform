/*
 * log_ringbuf — RAM circular log buffer with line-oriented retrieval.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    none
 */

#ifndef LOG_RINGBUF_H
#define LOG_RINGBUF_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    size_t   capacity;
    size_t   head;
    size_t   count;
    uint32_t total_lines;
    uint32_t dropped_lines;
} log_ringbuf_t;

/* @thread_safe: no — call from single context or protect externally */
void log_ringbuf_init(log_ringbuf_t *rb, uint8_t *buf, size_t capacity);

/* @thread_safe: no */
void log_ringbuf_write(log_ringbuf_t *rb, const char *line, size_t len);

/* Sink adapter: compatible with log_sink_fn signature.
 * Pass log_ringbuf_t* as ctx when registering via log_add_sink().
 * @thread_safe: no */
void log_ringbuf_sink(void *ctx, const char *line, size_t len);

/* Read the entire buffer content into dst. Returns bytes written.
 * Output is oldest-first, newline-separated.
 * @thread_safe: no */
size_t log_ringbuf_read(const log_ringbuf_t *rb, char *dst, size_t dst_size);

/* Return number of bytes currently stored.
 * @thread_safe: no */
size_t log_ringbuf_used(const log_ringbuf_t *rb);

/* Return total lines ever written (including dropped).
 * @thread_safe: no */
uint32_t log_ringbuf_total(const log_ringbuf_t *rb);

/* Return lines dropped due to buffer full.
 * @thread_safe: no */
uint32_t log_ringbuf_dropped(const log_ringbuf_t *rb);

/* Clear all stored data.
 * @thread_safe: no */
void log_ringbuf_clear(log_ringbuf_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* LOG_RINGBUF_H */
