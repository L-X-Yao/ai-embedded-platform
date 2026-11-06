// SPDX-License-Identifier: MIT
/*
 * event_queue — fixed-size circular event queue
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "event_queue.h"
#include "contract.h"

#define EQ_INVARIANT(q) do { \
    INVARIANT((q)->head < (q)->capacity); \
    INVARIANT((q)->tail < (q)->capacity); \
} while(0)

event_queue_err_t event_queue_init(event_queue_t *q, event_t *buf, size_t capacity)
{
    if (!q || !buf || capacity == 0)
    {
        return EVENT_QUEUE_ERR_PARAM;
    }
    q->buf = buf;
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    ENSURE(q->head == 0 && q->tail == 0);
    return EVENT_QUEUE_OK;
}

event_queue_err_t event_queue_push(event_queue_t *q, uint16_t type, uint32_t data)
{
    if (!q || !q->buf || q->capacity == 0)
    {
        return EVENT_QUEUE_ERR_PARAM;
    }

    size_t next = (q->head + 1) % q->capacity;
    if (next == q->tail)
    {
        return EVENT_QUEUE_ERR_FULL;
    }

    q->buf[q->head].type = type;
    q->buf[q->head].len = 0;
    q->buf[q->head].data = data;
    q->head = next;
    EQ_INVARIANT(q);

    return EVENT_QUEUE_OK;
}

event_queue_err_t event_queue_pop(event_queue_t *q, event_t *out)
{
    if (!q || !out || !q->buf || q->capacity == 0)
    {
        return EVENT_QUEUE_ERR_PARAM;
    }
    if (q->tail == q->head)
    {
        return EVENT_QUEUE_ERR_EMPTY;
    }

    *out = q->buf[q->tail];
    q->tail = (q->tail + 1) % q->capacity;
    EQ_INVARIANT(q);

    return EVENT_QUEUE_OK;
}

event_queue_err_t event_queue_peek(const event_queue_t *q, event_t *out)
{
    if (!q || !out)
    {
        return EVENT_QUEUE_ERR_PARAM;
    }
    if (q->tail == q->head)
    {
        return EVENT_QUEUE_ERR_EMPTY;
    }

    *out = q->buf[q->tail];
    return EVENT_QUEUE_OK;
}

size_t event_queue_count(const event_queue_t *q)
{
    if (!q)
    {
        return 0;
    }
    /* One read of each: a push or pop can move them between two reads. */
    size_t head = q->head;
    size_t tail = q->tail;
    if (head >= tail)
    {
        return head - tail;
    }
    return q->capacity - tail + head;
}

bool event_queue_is_empty(const event_queue_t *q)
{
    if (!q)
    {
        return true;
    }
    return q->head == q->tail;
}

bool event_queue_is_full(const event_queue_t *q)
{
    if (!q || !q->buf || q->capacity == 0)
    {
        return false;
    }
    return ((q->head + 1) % q->capacity) == q->tail;
}

event_queue_err_t event_queue_clear(event_queue_t *q)
{
    if (!q)
    {
        return EVENT_QUEUE_ERR_PARAM;
    }
    q->head = 0;
    q->tail = 0;
    return EVENT_QUEUE_OK;
}
