// SPDX-License-Identifier: MIT
/*
 * ring_buffer — byte-level circular buffer for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ring_buffer.h"
#include "contract.h"
#include <string.h>

#define RB_INVARIANT(rb) do { \
    INVARIANT((rb)->head < 2U * (uint32_t)(rb)->capacity); \
    INVARIANT((rb)->tail < 2U * (uint32_t)(rb)->capacity); \
} while(0)

/*
 * Bytes between two index values. A side always gets the exact level, because
 * it owns one index and the other index cannot pass it. Only a third context,
 * reading the two at different moments, can see more than capacity.
 */
static uint16_t rb_level(uint32_t head, uint32_t tail, uint32_t capacity)
{
    uint32_t level = (head >= tail) ? head - tail : head + 2U * capacity - tail;

    return (uint16_t)((level > capacity) ? capacity : level);
}

/* Place in buf for an index that runs over 0 .. 2*capacity-1. */
static uint32_t rb_position(uint32_t index, uint32_t capacity)
{
    return (index >= capacity) ? index - capacity : index;
}

static uint32_t rb_next_position(uint32_t pos, uint32_t capacity)
{
    return (pos + 1U == capacity) ? 0U : pos + 1U;
}

static uint32_t rb_advance(uint32_t index, uint16_t n, uint32_t capacity)
{
    uint32_t next = index + n;

    return (next >= 2U * capacity) ? next - 2U * capacity : next;
}

/* Copy up to len bytes out from the read index, and return how many. */
static uint16_t rb_copy_out(const ring_buffer_t *rb, uint32_t tail,
                            uint8_t *data, uint16_t len)
{
    uint32_t capacity = rb->capacity;
    uint16_t level = rb_level(rb->head, tail, capacity);

    if (len > level) { len = level; }

    RING_BUFFER_BARRIER(); /* head announced these bytes: read them after it */

    uint32_t pos = rb_position(tail, capacity);
    for (uint16_t i = 0; i < len; i++) {
        data[i] = rb->buf[pos];
        pos = rb_next_position(pos, capacity);
    }
    return len;
}

ring_buffer_err_t ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint16_t capacity)
{
    if (!rb || !buf || capacity == 0) {
        return RING_BUFFER_ERR_PARAM;
    }
    rb->buf      = buf;
    rb->capacity = capacity;
    rb->head     = 0;
    rb->tail     = 0;
    ENSURE(rb->head == 0 && rb->tail == 0);
    return RING_BUFFER_OK;
}

uint16_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    if (!rb || !data) { return 0; }
    RB_INVARIANT(rb);

    /* One read of each index: the consumer can move tail at any moment. */
    uint32_t capacity = rb->capacity;
    uint32_t head = rb->head;
    uint16_t avail = (uint16_t)(capacity - rb_level(head, rb->tail, capacity));

    if (len > avail) { len = avail; }

    RING_BUFFER_BARRIER(); /* tail freed these places: write them after it */

    uint32_t pos = rb_position(head, capacity);
    for (uint16_t i = 0; i < len; i++) {
        rb->buf[pos] = data[i];
        pos = rb_next_position(pos, capacity);
    }

    RING_BUFFER_BARRIER(); /* the bytes land before head announces them */
    rb->head = rb_advance(head, len, capacity);
    RB_INVARIANT(rb);
    return len;
}

uint16_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (!rb || !data) { return 0; }
    RB_INVARIANT(rb);

    uint32_t tail = rb->tail;

    len = rb_copy_out(rb, tail, data, len);

    RING_BUFFER_BARRIER(); /* the bytes are out before tail frees their places */
    rb->tail = rb_advance(tail, len, rb->capacity);
    RB_INVARIANT(rb);
    return len;
}

uint16_t ring_buffer_peek(const ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (!rb || !data) { return 0; }

    return rb_copy_out(rb, rb->tail, data, len);
}

uint16_t ring_buffer_skip(ring_buffer_t *rb, uint16_t len)
{
    if (!rb) { return 0; }

    uint32_t capacity = rb->capacity;
    uint32_t tail = rb->tail;
    uint16_t level = rb_level(rb->head, tail, capacity);

    if (len > level) { len = level; }
    if (len == 0U) { return 0; }

    RING_BUFFER_BARRIER(); /* a peek before this call must finish first */
    rb->tail = rb_advance(tail, len, capacity);
    return len;
}

uint16_t ring_buffer_count(const ring_buffer_t *rb)
{
    if (!rb) { return 0; }
    return rb_level(rb->head, rb->tail, rb->capacity);
}

uint16_t ring_buffer_free(const ring_buffer_t *rb)
{
    if (!rb) { return 0; }

    uint32_t capacity = rb->capacity;

    return (uint16_t)(capacity - rb_level(rb->head, rb->tail, capacity));
}

bool ring_buffer_is_empty(const ring_buffer_t *rb)
{
    if (!rb) { return true; }
    return rb->head == rb->tail;
}

bool ring_buffer_is_full(const ring_buffer_t *rb)
{
    if (!rb || rb->capacity == 0) { return false; }
    return rb_level(rb->head, rb->tail, rb->capacity) == rb->capacity;
}

void ring_buffer_reset(ring_buffer_t *rb)
{
    if (!rb) { return; }

    RING_BUFFER_BARRIER(); /* a peek before this call must finish first */
    rb->tail = rb->head;
}
