// SPDX-License-Identifier: MIT
/*
 * byte_buffer — linear read/write byte buffer for protocol framing
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Linear buffer with separate read and write cursors. Useful for
 * protocol packet assembly, UART RX accumulation, and serialization.
 * Caller provides the backing storage. No heap, fully reentrant.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BYTE_BUFFER_OK        =  0,
    BYTE_BUFFER_ERR_PARAM = -1,
    BYTE_BUFFER_ERR_FULL  = -2,
    BYTE_BUFFER_ERR_EMPTY = -3,
} byte_buffer_err_t;

typedef struct {
    uint8_t *data;
    size_t   capacity;
    size_t   write_pos;
    size_t   read_pos;
} byte_buffer_t;

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_init(byte_buffer_t *buf, uint8_t *storage, size_t capacity);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_reset(byte_buffer_t *buf);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_put_u8(byte_buffer_t *buf, uint8_t val);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_put_u16_be(byte_buffer_t *buf, uint16_t val);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_put_u32_be(byte_buffer_t *buf, uint32_t val);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_put_bytes(byte_buffer_t *buf, const uint8_t *src, size_t len);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_get_u8(byte_buffer_t *buf, uint8_t *val);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_get_u16_be(byte_buffer_t *buf, uint16_t *val);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_get_u32_be(byte_buffer_t *buf, uint32_t *val);

/** @thread_safe: no */
byte_buffer_err_t byte_buffer_get_bytes(byte_buffer_t *buf, uint8_t *dst, size_t len);

/** @thread_safe: no */
size_t byte_buffer_readable(const byte_buffer_t *buf);

/** @thread_safe: no */
size_t byte_buffer_writable(const byte_buffer_t *buf);

/** @thread_safe: no */
const uint8_t *byte_buffer_read_ptr(const byte_buffer_t *buf);

/** @thread_safe: no */
size_t byte_buffer_written(const byte_buffer_t *buf);

/** Compact: move unread data to start of buffer. @thread_safe: no */
byte_buffer_err_t byte_buffer_compact(byte_buffer_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* BYTE_BUFFER_H */
