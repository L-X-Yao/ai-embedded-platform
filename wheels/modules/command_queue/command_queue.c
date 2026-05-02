/*
 * command_queue — prioritized command queue for task dispatching
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 *
 * Insertion-sorted array: index 0 is always the highest-priority item
 * (lowest numeric priority value).  Equal priorities preserve FIFO order
 * because new items are inserted *after* existing items of the same
 * priority.
 */

#include "command_queue.h"
#include <string.h>

command_queue_err_t command_queue_init(command_queue_t *q,
                                      command_queue_item_t *buf,
                                      size_t capacity)
{
    if (!q || !buf || capacity == 0) {
        return COMMAND_QUEUE_ERR_PARAM;
    }
    q->buf      = buf;
    q->capacity = capacity;
    q->count    = 0;
    return COMMAND_QUEUE_OK;
}

command_queue_err_t command_queue_push(command_queue_t *q,
                                      uint16_t cmd_id,
                                      uint8_t priority,
                                      const void *data,
                                      uint8_t len)
{
    if (!q || !q->buf) {
        return COMMAND_QUEUE_ERR_PARAM;
    }
    if (len > COMMAND_QUEUE_DATA_MAX) {
        return COMMAND_QUEUE_ERR_PARAM;
    }
    if (data == NULL && len > 0) {
        return COMMAND_QUEUE_ERR_PARAM;
    }
    if (q->count >= q->capacity) {
        return COMMAND_QUEUE_ERR_FULL;
    }

    /* Find insertion point: first item with strictly greater priority value.
     * Items with equal priority stay before us → FIFO among same priority. */
    size_t pos = q->count;
    for (size_t i = 0; i < q->count; i++) {
        if (q->buf[i].priority > priority) {
            pos = i;
            break;
        }
    }

    /* Shift items right to make room */
    if (pos < q->count) {
        memmove(&q->buf[pos + 1], &q->buf[pos],
                (q->count - pos) * sizeof(command_queue_item_t));
    }

    /* Fill the slot */
    q->buf[pos].cmd_id   = cmd_id;
    q->buf[pos].priority = priority;
    q->buf[pos].len      = len;
    if (len > 0) {
        memcpy(q->buf[pos].data, data, len);
    }

    q->count++;
    return COMMAND_QUEUE_OK;
}

command_queue_err_t command_queue_pop(command_queue_t *q,
                                     command_queue_item_t *out)
{
    if (!q || !q->buf || !out) {
        return COMMAND_QUEUE_ERR_PARAM;
    }
    if (q->count == 0) {
        return COMMAND_QUEUE_ERR_EMPTY;
    }

    /* Copy highest-priority item (index 0) */
    *out = q->buf[0];

    /* Shift remaining items left */
    q->count--;
    if (q->count > 0) {
        memmove(&q->buf[0], &q->buf[1],
                q->count * sizeof(command_queue_item_t));
    }

    return COMMAND_QUEUE_OK;
}

command_queue_err_t command_queue_peek(const command_queue_t *q,
                                      command_queue_item_t *out)
{
    if (!q || !q->buf || !out) {
        return COMMAND_QUEUE_ERR_PARAM;
    }
    if (q->count == 0) {
        return COMMAND_QUEUE_ERR_EMPTY;
    }
    *out = q->buf[0];
    return COMMAND_QUEUE_OK;
}

size_t command_queue_count(const command_queue_t *q)
{
    if (!q || !q->buf) {
        return 0;
    }
    return q->count;
}

command_queue_err_t command_queue_clear(command_queue_t *q)
{
    if (!q || !q->buf) {
        return COMMAND_QUEUE_ERR_PARAM;
    }
    q->count = 0;
    return COMMAND_QUEUE_OK;
}

bool command_queue_is_empty(const command_queue_t *q)
{
    if (!q || !q->buf) {
        return true;
    }
    return q->count == 0;
}

bool command_queue_is_full(const command_queue_t *q)
{
    if (!q || !q->buf) {
        return false;
    }
    return q->count >= q->capacity;
}
