// SPDX-License-Identifier: MIT
/*
 * s_record — Motorola S-Record (SREC) firmware format parser
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Parses S-Record lines: S0 (header), S1/S2/S3 (data with 16/24/32-bit
 * address), S5 (record count), S7/S8/S9 (end). Checksum validation.
 * Useful for bootloaders and firmware update protocols.
 * Pure C99, no heap, no OS.
 */

#ifndef S_RECORD_H
#define S_RECORD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S_RECORD_MAX_DATA 32

typedef enum {
    S_RECORD_OK           =  0,
    S_RECORD_ERR_PARAM    = -1,
    S_RECORD_ERR_FORMAT   = -2,
    S_RECORD_ERR_SHORT    = -3,
    S_RECORD_ERR_OVERFLOW = -4,
    S_RECORD_ERR_CHECKSUM = -5,
} s_record_err_t;

typedef enum {
    S_RECORD_S0 = 0,
    S_RECORD_S1 = 1,
    S_RECORD_S2 = 2,
    S_RECORD_S3 = 3,
    S_RECORD_S5 = 5,
    S_RECORD_S7 = 7,
    S_RECORD_S8 = 8,
    S_RECORD_S9 = 9,
} s_record_type_t;

typedef struct {
    s_record_type_t type;
    uint32_t        address;
    uint8_t         data[S_RECORD_MAX_DATA];
    uint8_t         data_len;
} s_record_line_t;

/** Decode one S-Record line (ASCII text). @thread_safe: yes @isr_safe: yes */
s_record_err_t s_record_decode(const char *line, size_t len,
                                s_record_line_t *out);

/**
 * Encode one S-Record line to ASCII. Returns bytes written via out_len.
 * @thread_safe: yes @isr_safe: yes
 */
s_record_err_t s_record_encode(const s_record_line_t *rec,
                                char *buf, size_t cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* S_RECORD_H */
