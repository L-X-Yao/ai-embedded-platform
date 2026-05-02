// SPDX-License-Identifier: MIT
/*
 * ring_buffer — byte-level circular buffer for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Circular buffer with head/tail pointers and explicit count.
 * Supports partial write/read, peek, and skip. Suitable for
 * UART RX buffering, protocol framing, and producer-consumer
 * byte streams. Caller provides backing storage. No heap.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RING_BUFFER_OK         =  0,
    RING_BUFFER_ERR_PARAM = -1,
    RING_BUFFER_ERR_FULL   = -2,
    RING_BUFFER_ERR_EMPTY  = -3,
} ring_buffer_err_t;

typedef struct {
    uint8_t  *buf;      /* backing storage (caller-owned) */
    uint16_t  capacity; /* total size in bytes             */
    uint16_t  head;     /* next write position             */
    uint16_t  tail;     /* next read position              */
    uint16_t  count;    /* bytes currently stored           */
} ring_buffer_t;

/** Initialize ring buffer with caller-provided storage. @thread_safe: no */
ring_buffer_err_t ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint16_t capacity);

/** Write up to len bytes. Returns bytes actually written. @thread_safe: no */
uint16_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint16_t len);

/** Read up to len bytes (consuming). Returns bytes actually read. @thread_safe: no */
uint16_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint16_t len);

/** Read up to len bytes without consuming. Returns bytes peeked. @thread_safe: no */
uint16_t ring_buffer_peek(const ring_buffer_t *rb, uint8_t *data, uint16_t len);

/** Discard up to len bytes. Returns bytes actually skipped. @thread_safe: no */
uint16_t ring_buffer_skip(ring_buffer_t *rb, uint16_t len);

/** Return number of bytes currently stored. @thread_safe: no */
uint16_t ring_buffer_count(const ring_buffer_t *rb);

/** Return number of free bytes. @thread_safe: no */
uint16_t ring_buffer_free(const ring_buffer_t *rb);

/** Return true if buffer is empty. @thread_safe: no */
bool ring_buffer_is_empty(const ring_buffer_t *rb);

/** Return true if buffer is full. @thread_safe: no */
bool ring_buffer_is_full(const ring_buffer_t *rb);

/** Reset buffer to empty state. @thread_safe: no */
void ring_buffer_reset(ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
