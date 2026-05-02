// SPDX-License-Identifier: MIT
/*
 * command_queue — prioritized command queue for task dispatching
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 *
 * Fixed-size priority queue for dispatching commands.  Commands are
 * insertion-sorted by priority (lower numeric value = higher priority).
 * Commands with equal priority are dequeued in FIFO order.
 * Caller provides the backing array — no heap, fully reentrant per-context.
 */

#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMMAND_QUEUE_DATA_MAX
#define COMMAND_QUEUE_DATA_MAX  16
#endif

typedef enum {
    COMMAND_QUEUE_OK        =  0,
    COMMAND_QUEUE_ERR_PARAM = -1,
    COMMAND_QUEUE_ERR_FULL  = -2,
    COMMAND_QUEUE_ERR_EMPTY = -3,
} command_queue_err_t;

typedef struct {
    uint16_t cmd_id;
    uint8_t  priority;          /* lower = higher priority */
    uint8_t  len;               /* payload bytes (0..COMMAND_QUEUE_DATA_MAX) */
    uint8_t  data[COMMAND_QUEUE_DATA_MAX];
} command_queue_item_t;

typedef struct {
    command_queue_item_t *buf;  /* caller-provided backing array */
    size_t capacity;            /* max number of items */
    size_t count;               /* current number of items */
} command_queue_t;

/** @thread_safe: no */
command_queue_err_t command_queue_init(command_queue_t *q,
                                      command_queue_item_t *buf,
                                      size_t capacity);

/** @thread_safe: no — insertion-sorts by priority */
command_queue_err_t command_queue_push(command_queue_t *q,
                                      uint16_t cmd_id,
                                      uint8_t priority,
                                      const void *data,
                                      uint8_t len);

/** @thread_safe: no — removes highest-priority (index 0) */
command_queue_err_t command_queue_pop(command_queue_t *q,
                                     command_queue_item_t *out);

/** @thread_safe: yes (read only) */
command_queue_err_t command_queue_peek(const command_queue_t *q,
                                      command_queue_item_t *out);

/** @thread_safe: yes (read only) */
size_t command_queue_count(const command_queue_t *q);

/** @thread_safe: no */
command_queue_err_t command_queue_clear(command_queue_t *q);

/** @thread_safe: yes (read only) */
bool command_queue_is_empty(const command_queue_t *q);

/** @thread_safe: yes (read only) */
bool command_queue_is_full(const command_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_QUEUE_H */
