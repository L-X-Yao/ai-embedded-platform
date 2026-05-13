// SPDX-License-Identifier: MIT
/*
 * modbus_ascii — Modbus ASCII frame encode/decode
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Frame: ':' + hex(addr) + hex(func) + hex(data...) + hex(LRC) + CR LF
 * LRC = two's complement of sum of binary bytes (addr+func+data).
 * Complements modbus_rtu (binary) and modbus_tcp (TCP wrapper).
 * Pure C99, no heap, no OS.
 */

#ifndef MODBUS_ASCII_H
#define MODBUS_ASCII_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODBUS_ASCII_MAX_DATA 252

typedef enum {
    MODBUS_ASCII_OK           =  0,
    MODBUS_ASCII_ERR_PARAM    = -1,
    MODBUS_ASCII_ERR_FORMAT   = -2,
    MODBUS_ASCII_ERR_SHORT    = -3,
    MODBUS_ASCII_ERR_OVERFLOW = -4,
    MODBUS_ASCII_ERR_LRC      = -5,
} modbus_ascii_err_t;

typedef struct {
    uint8_t addr;
    uint8_t func;
    uint8_t data[MODBUS_ASCII_MAX_DATA];
    uint8_t data_len;
} modbus_ascii_frame_t;

/** Calculate LRC over binary data. @thread_safe: yes @isr_safe: yes */
uint8_t modbus_ascii_lrc(const uint8_t *data, size_t len);

/**
 * Decode an ASCII frame (including ':' and CRLF).
 * @thread_safe: yes @isr_safe: yes
 */
modbus_ascii_err_t modbus_ascii_decode(const char *ascii, size_t len,
                                        modbus_ascii_frame_t *out);

/**
 * Encode a frame to ASCII string (including ':' and CRLF).
 * Returns total bytes written via out_len.
 * @thread_safe: yes @isr_safe: yes
 */
modbus_ascii_err_t modbus_ascii_encode(const modbus_ascii_frame_t *frame,
                                        char *buf, size_t cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_ASCII_H */
