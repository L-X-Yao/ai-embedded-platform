// SPDX-License-Identifier: MIT
/*
 * event_queue — fixed-size circular event queue
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Circular FIFO queue for typed events with optional data payload.
 * Suitable for ISR-to-mainloop communication and bare-metal event-driven
 * architectures. Caller provides the backing array.
 * No heap, fully reentrant per-context.
 * Thread safety: single-producer single-consumer safe (ISR push, main pop).
 */

#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_QUEUE_OK        =  0,
    EVENT_QUEUE_ERR_PARAM = -1,
    EVENT_QUEUE_ERR_FULL  = -2,
    EVENT_QUEUE_ERR_EMPTY = -3,
} event_queue_err_t;

typedef struct {
    uint16_t type;
    uint16_t len;
    uint32_t data;
} event_t;

typedef struct {
    event_t *buf;
    size_t   capacity;
    volatile size_t head;
    volatile size_t tail;
} event_queue_t;

/** @thread_safe: no @isr_safe: no */
event_queue_err_t event_queue_init(event_queue_t *q, event_t *buf, size_t capacity);

/** @thread_safe: single-producer safe (can call from ISR) @isr_safe: yes */
event_queue_err_t event_queue_push(event_queue_t *q, uint16_t type, uint32_t data);

/** @thread_safe: single-consumer safe (call from main loop) @isr_safe: yes */
event_queue_err_t event_queue_pop(event_queue_t *q, event_t *out);

/** @thread_safe: yes (read only) @isr_safe: yes */
event_queue_err_t event_queue_peek(const event_queue_t *q, event_t *out);

/** @thread_safe: yes (read only) @isr_safe: yes */
size_t event_queue_count(const event_queue_t *q);

/** @thread_safe: yes (read only) @isr_safe: yes */
bool event_queue_is_empty(const event_queue_t *q);

/** @thread_safe: yes (read only) @isr_safe: yes */
bool event_queue_is_full(const event_queue_t *q);

/** @thread_safe: no @isr_safe: yes */
event_queue_err_t event_queue_clear(event_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_QUEUE_H */
