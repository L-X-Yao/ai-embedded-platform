// SPDX-License-Identifier: MIT
/*
 * byte_buffer — linear read/write byte buffer for protocol framing
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "byte_buffer.h"
#include <string.h>

byte_buffer_err_t byte_buffer_init(byte_buffer_t *buf, uint8_t *storage, size_t capacity)
{
    if (!buf || !storage || capacity == 0)
    {
        return BYTE_BUFFER_ERR_PARAM;
    }
    buf->data = storage;
    buf->capacity = capacity;
    buf->write_pos = 0;
    buf->read_pos = 0;
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_reset(byte_buffer_t *buf)
{
    if (!buf)
    {
        return BYTE_BUFFER_ERR_PARAM;
    }
    buf->write_pos = 0;
    buf->read_pos = 0;
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_put_u8(byte_buffer_t *buf, uint8_t val)
{
    if (!buf) return BYTE_BUFFER_ERR_PARAM;
    if (buf->write_pos >= buf->capacity) return BYTE_BUFFER_ERR_FULL;
    buf->data[buf->write_pos++] = val;
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_put_u16_be(byte_buffer_t *buf, uint16_t val)
{
    if (!buf) return BYTE_BUFFER_ERR_PARAM;
    if (buf->capacity - buf->write_pos < 2) return BYTE_BUFFER_ERR_FULL;
    buf->data[buf->write_pos++] = (uint8_t)(val >> 8);
    buf->data[buf->write_pos++] = (uint8_t)(val & 0xFF);
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_put_u32_be(byte_buffer_t *buf, uint32_t val)
{
    if (!buf) return BYTE_BUFFER_ERR_PARAM;
    if (buf->capacity - buf->write_pos < 4) return BYTE_BUFFER_ERR_FULL;
    buf->data[buf->write_pos++] = (uint8_t)(val >> 24);
    buf->data[buf->write_pos++] = (uint8_t)(val >> 16);
    buf->data[buf->write_pos++] = (uint8_t)(val >> 8);
    buf->data[buf->write_pos++] = (uint8_t)(val & 0xFF);
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_put_bytes(byte_buffer_t *buf, const uint8_t *src, size_t len)
{
    if (!buf || (!src && len > 0)) return BYTE_BUFFER_ERR_PARAM;
    if (buf->capacity - buf->write_pos < len) return BYTE_BUFFER_ERR_FULL;
    memcpy(buf->data + buf->write_pos, src, len);
    buf->write_pos += len;
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_get_u8(byte_buffer_t *buf, uint8_t *val)
{
    if (!buf || !val) return BYTE_BUFFER_ERR_PARAM;
    if (buf->read_pos >= buf->write_pos) return BYTE_BUFFER_ERR_EMPTY;
    *val = buf->data[buf->read_pos++];
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_get_u16_be(byte_buffer_t *buf, uint16_t *val)
{
    if (!buf || !val) return BYTE_BUFFER_ERR_PARAM;
    if (buf->write_pos - buf->read_pos < 2) return BYTE_BUFFER_ERR_EMPTY;
    *val = (uint16_t)(((uint16_t)buf->data[buf->read_pos] << 8) |
                      (uint16_t)buf->data[buf->read_pos + 1]);
    buf->read_pos += 2;
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_get_u32_be(byte_buffer_t *buf, uint32_t *val)
{
    if (!buf || !val) return BYTE_BUFFER_ERR_PARAM;
    if (buf->write_pos - buf->read_pos < 4) return BYTE_BUFFER_ERR_EMPTY;
    *val = ((uint32_t)buf->data[buf->read_pos] << 24)
         | ((uint32_t)buf->data[buf->read_pos + 1] << 16)
         | ((uint32_t)buf->data[buf->read_pos + 2] << 8)
         | ((uint32_t)buf->data[buf->read_pos + 3]);
    buf->read_pos += 4;
    return BYTE_BUFFER_OK;
}

byte_buffer_err_t byte_buffer_get_bytes(byte_buffer_t *buf, uint8_t *dst, size_t len)
{
    if (!buf || (!dst && len > 0)) return BYTE_BUFFER_ERR_PARAM;
    if (buf->write_pos - buf->read_pos < len) return BYTE_BUFFER_ERR_EMPTY;
    memcpy(dst, buf->data + buf->read_pos, len);
    buf->read_pos += len;
    return BYTE_BUFFER_OK;
}

size_t byte_buffer_readable(const byte_buffer_t *buf)
{
    if (!buf) return 0;
    return buf->write_pos - buf->read_pos;
}

size_t byte_buffer_writable(const byte_buffer_t *buf)
{
    if (!buf) return 0;
    return buf->capacity - buf->write_pos;
}

const uint8_t *byte_buffer_read_ptr(const byte_buffer_t *buf)
{
    if (!buf) return NULL;
    return buf->data + buf->read_pos;
}

size_t byte_buffer_written(const byte_buffer_t *buf)
{
    if (!buf) return 0;
    return buf->write_pos;
}

byte_buffer_err_t byte_buffer_compact(byte_buffer_t *buf)
{
    if (!buf) return BYTE_BUFFER_ERR_PARAM;
    size_t unread = buf->write_pos - buf->read_pos;
    if (buf->read_pos > 0 && unread > 0)
    {
        memmove(buf->data, buf->data + buf->read_pos, unread);
    }
    buf->read_pos = 0;
    buf->write_pos = unread;
    return BYTE_BUFFER_OK;
}
