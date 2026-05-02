/*
 * fifo_queue — typed fixed-size FIFO queue with caller-provided storage
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "fifo_queue.h"
#include <string.h>

fifo_err_t fifo_queue_init(fifo_queue_t *q, void *storage,
                           uint16_t capacity, uint16_t elem_size)
{
    if (!q || !storage || capacity == 0 || elem_size == 0)
    {
        return FIFO_ERR_PARAMS;
    }
    q->buf       = (uint8_t *)storage;
    q->capacity  = capacity;
    q->elem_size = elem_size;
    q->head      = 0;
    q->tail      = 0;
    q->count     = 0;
    return FIFO_OK;
}

fifo_err_t fifo_queue_push(fifo_queue_t *q, const void *item)
{
    if (!q || !item || !q->buf)
    {
        return FIFO_ERR_PARAMS;
    }
    if (q->count >= q->capacity)
    {
        return FIFO_ERR_FULL;
    }
    memcpy(q->buf + (uint32_t)q->tail * q->elem_size, item, q->elem_size);
    q->tail = (uint16_t)((q->tail + 1U) % q->capacity);
    q->count++;
    return FIFO_OK;
}

fifo_err_t fifo_queue_pop(fifo_queue_t *q, void *item)
{
    if (!q || !item || !q->buf)
    {
        return FIFO_ERR_PARAMS;
    }
    if (q->count == 0)
    {
        return FIFO_ERR_EMPTY;
    }
    memcpy(item, q->buf + (uint32_t)q->head * q->elem_size, q->elem_size);
    q->head = (uint16_t)((q->head + 1U) % q->capacity);
    q->count--;
    return FIFO_OK;
}

fifo_err_t fifo_queue_peek(const fifo_queue_t *q, void *item)
{
    if (!q || !item || !q->buf)
    {
        return FIFO_ERR_PARAMS;
    }
    if (q->count == 0)
    {
        return FIFO_ERR_EMPTY;
    }
    memcpy(item, q->buf + (uint32_t)q->head * q->elem_size, q->elem_size);
    return FIFO_OK;
}

uint16_t fifo_queue_count(const fifo_queue_t *q)
{
    if (!q) return 0;
    return q->count;
}

uint16_t fifo_queue_free(const fifo_queue_t *q)
{
    if (!q) return 0;
    return (uint16_t)(q->capacity - q->count);
}

bool fifo_queue_is_empty(const fifo_queue_t *q)
{
    if (!q) return true;
    return q->count == 0;
}

bool fifo_queue_is_full(const fifo_queue_t *q)
{
    if (!q || q->capacity == 0) return false;
    return q->count >= q->capacity;
}

void fifo_queue_clear(fifo_queue_t *q)
{
    if (!q) return;
    q->head  = 0;
    q->tail  = 0;
    q->count = 0;
}
