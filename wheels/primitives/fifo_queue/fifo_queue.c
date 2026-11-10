// SPDX-License-Identifier: MIT
/*
 * fifo_queue — typed fixed-size FIFO queue with caller-provided storage
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "fifo_queue.h"
#include "contract.h"
#include <string.h>

#define FQ_INVARIANT(q) do { \
    INVARIANT((q)->head < 2U * (uint32_t)(q)->capacity); \
    INVARIANT((q)->tail < 2U * (uint32_t)(q)->capacity); \
} while(0)

/*
 * Elements between two index values. A side always gets the exact level,
 * because it owns one index and the other index cannot pass it. Only a third
 * context, reading the two at different moments, can see more than capacity.
 */
static uint16_t fq_level(uint32_t head, uint32_t tail, uint32_t capacity)
{
    uint32_t level = (tail >= head) ? tail - head : tail + 2U * capacity - head;

    return (uint16_t)((level > capacity) ? capacity : level);
}

/* Byte offset in buf for an index that runs over 0 .. 2*capacity-1. */
static uint32_t fq_offset(const fifo_queue_t *q, uint32_t index)
{
    uint32_t capacity = q->capacity;
    uint32_t slot = (index >= capacity) ? index - capacity : index;

    return slot * q->elem_size;
}

static uint32_t fq_next(uint32_t index, uint32_t capacity)
{
    return (index + 1U == 2U * capacity) ? 0U : index + 1U;
}

/* Copy out the oldest element, or say there is none. head is the caller's. */
static fifo_queue_err_t fq_copy_out(const fifo_queue_t *q, uint32_t head, void *item)
{
    if (q->tail == head)
    {
        return FIFO_QUEUE_ERR_EMPTY;
    }
    FIFO_QUEUE_BARRIER(); /* tail announced this element: read it after that */
    memcpy(item, q->buf + fq_offset(q, head), q->elem_size);
    return FIFO_QUEUE_OK;
}

fifo_queue_err_t fifo_queue_init(fifo_queue_t *q, void *storage,
                           uint16_t capacity, uint16_t elem_size)
{
    if (!q || !storage || capacity == 0 || elem_size == 0)
    {
        return FIFO_QUEUE_ERR_PARAM;
    }
    q->buf       = (uint8_t *)storage;
    q->capacity  = capacity;
    q->elem_size = elem_size;
    q->head      = 0;
    q->tail      = 0;
    ENSURE(q->head == 0 && q->tail == 0);
    return FIFO_QUEUE_OK;
}

fifo_queue_err_t fifo_queue_push(fifo_queue_t *q, const void *item)
{
    if (!q || !item || !q->buf)
    {
        return FIFO_QUEUE_ERR_PARAM;
    }

    /* One read of each index: the consumer can move head at any moment. */
    uint32_t capacity = q->capacity;
    uint32_t tail = q->tail;

    if (fq_level(q->head, tail, capacity) >= capacity)
    {
        return FIFO_QUEUE_ERR_FULL;
    }

    FIFO_QUEUE_BARRIER(); /* head freed this slot: write it after that */
    memcpy(q->buf + fq_offset(q, tail), item, q->elem_size);
    FIFO_QUEUE_BARRIER(); /* the element lands before tail announces it */
    q->tail = fq_next(tail, capacity);
    FQ_INVARIANT(q);
    return FIFO_QUEUE_OK;
}

fifo_queue_err_t fifo_queue_pop(fifo_queue_t *q, void *item)
{
    if (!q || !item || !q->buf)
    {
        return FIFO_QUEUE_ERR_PARAM;
    }

    uint32_t head = q->head;
    fifo_queue_err_t err = fq_copy_out(q, head, item);

    if (err != FIFO_QUEUE_OK)
    {
        return err;
    }
    FIFO_QUEUE_BARRIER(); /* the element is out before head frees its slot */
    q->head = fq_next(head, q->capacity);
    FQ_INVARIANT(q);
    return FIFO_QUEUE_OK;
}

fifo_queue_err_t fifo_queue_peek(const fifo_queue_t *q, void *item)
{
    if (!q || !item || !q->buf)
    {
        return FIFO_QUEUE_ERR_PARAM;
    }
    return fq_copy_out(q, q->head, item);
}

uint16_t fifo_queue_count(const fifo_queue_t *q)
{
    if (!q) return 0;
    return fq_level(q->head, q->tail, q->capacity);
}

uint16_t fifo_queue_free(const fifo_queue_t *q)
{
    if (!q) return 0;

    uint32_t capacity = q->capacity;

    return (uint16_t)(capacity - fq_level(q->head, q->tail, capacity));
}

bool fifo_queue_is_empty(const fifo_queue_t *q)
{
    if (!q) return true;
    return q->head == q->tail;
}

bool fifo_queue_is_full(const fifo_queue_t *q)
{
    if (!q || q->capacity == 0) return false;
    return fq_level(q->head, q->tail, q->capacity) >= q->capacity;
}

void fifo_queue_clear(fifo_queue_t *q)
{
    if (!q) return;

    FIFO_QUEUE_BARRIER(); /* a peek before this call must finish first */
    q->head = q->tail;
}
