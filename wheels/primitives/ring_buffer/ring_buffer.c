/*
 * ring_buffer — byte-level circular buffer for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "ring_buffer.h"
#include <string.h>

ring_buffer_err_t ring_buffer_init(ring_buffer_t *rb, uint8_t *buf, uint16_t capacity)
{
    if (!rb || !buf || capacity == 0) {
        return RING_BUFFER_ERR_PARAMS;
    }
    rb->buf      = buf;
    rb->capacity = capacity;
    rb->head     = 0;
    rb->tail     = 0;
    rb->count    = 0;
    return RING_BUFFER_OK;
}

uint16_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, uint16_t len)
{
    if (!rb || !data) { return 0; }

    uint16_t avail = (uint16_t)(rb->capacity - rb->count);
    if (len > avail) { len = avail; }

    for (uint16_t i = 0; i < len; i++) {
        rb->buf[rb->head] = data[i];
        rb->head = (uint16_t)((rb->head + 1U) % rb->capacity);
    }
    rb->count = (uint16_t)(rb->count + len);
    return len;
}

uint16_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (!rb || !data) { return 0; }

    if (len > rb->count) { len = rb->count; }

    for (uint16_t i = 0; i < len; i++) {
        data[i] = rb->buf[rb->tail];
        rb->tail = (uint16_t)((rb->tail + 1U) % rb->capacity);
    }
    rb->count = (uint16_t)(rb->count - len);
    return len;
}

uint16_t ring_buffer_peek(const ring_buffer_t *rb, uint8_t *data, uint16_t len)
{
    if (!rb || !data) { return 0; }

    if (len > rb->count) { len = rb->count; }

    uint16_t pos = rb->tail;
    for (uint16_t i = 0; i < len; i++) {
        data[i] = rb->buf[pos];
        pos = (uint16_t)((pos + 1U) % rb->capacity);
    }
    return len;
}

uint16_t ring_buffer_skip(ring_buffer_t *rb, uint16_t len)
{
    if (!rb) { return 0; }

    if (len > rb->count) { len = rb->count; }

    rb->tail  = (uint16_t)((rb->tail + len) % rb->capacity);
    rb->count = (uint16_t)(rb->count - len);
    return len;
}

uint16_t ring_buffer_count(const ring_buffer_t *rb)
{
    if (!rb) { return 0; }
    return rb->count;
}

uint16_t ring_buffer_free(const ring_buffer_t *rb)
{
    if (!rb) { return 0; }
    return (uint16_t)(rb->capacity - rb->count);
}

bool ring_buffer_is_empty(const ring_buffer_t *rb)
{
    if (!rb) { return true; }
    return rb->count == 0;
}

bool ring_buffer_is_full(const ring_buffer_t *rb)
{
    if (!rb || rb->capacity == 0) { return false; }
    return rb->count == rb->capacity;
}

void ring_buffer_reset(ring_buffer_t *rb)
{
    if (!rb) { return; }
    rb->head  = 0;
    rb->tail  = 0;
    rb->count = 0;
}
