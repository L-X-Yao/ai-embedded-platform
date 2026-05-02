// SPDX-License-Identifier: MIT
/*
 * fifo_queue — typed fixed-size FIFO queue with caller-provided storage
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Circular queue for fixed-size elements (structs, ints, etc.).
 * Complements byte_buffer (linear bytes) and ring_buffer (byte-level
 * circular). Caller provides backing storage. No heap, fully reentrant.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#ifndef FIFO_QUEUE_H
#define FIFO_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FIFO_QUEUE_OK         =  0,
    FIFO_QUEUE_ERR_PARAM = -1,
    FIFO_QUEUE_ERR_FULL   = -2,
    FIFO_QUEUE_ERR_EMPTY  = -3,
} fifo_queue_err_t;

typedef struct {
    uint8_t *buf;       /* backing storage (caller-owned) */
    uint16_t capacity;  /* max number of elements          */
    uint16_t elem_size; /* size of each element in bytes    */
    uint16_t head;      /* read index  (element count)      */
    uint16_t tail;      /* write index (element count)      */
    uint16_t count;     /* current number of elements       */
} fifo_queue_t;

/** @thread_safe: no */
fifo_queue_err_t fifo_queue_init(fifo_queue_t *q, void *storage,
                           uint16_t capacity, uint16_t elem_size);

/** @thread_safe: no */
fifo_queue_err_t fifo_queue_push(fifo_queue_t *q, const void *item);

/** @thread_safe: no */
fifo_queue_err_t fifo_queue_pop(fifo_queue_t *q, void *item);

/** @thread_safe: no */
fifo_queue_err_t fifo_queue_peek(const fifo_queue_t *q, void *item);

/** @thread_safe: no */
uint16_t fifo_queue_count(const fifo_queue_t *q);

/** @thread_safe: no */
uint16_t fifo_queue_free(const fifo_queue_t *q);

/** @thread_safe: no */
bool fifo_queue_is_empty(const fifo_queue_t *q);

/** @thread_safe: no */
bool fifo_queue_is_full(const fifo_queue_t *q);

/** @thread_safe: no */
void fifo_queue_clear(fifo_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* FIFO_QUEUE_H */
