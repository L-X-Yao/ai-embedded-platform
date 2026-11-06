// SPDX-License-Identifier: MIT
/*
 * intel_hex — Intel HEX firmware format parser
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Parses Intel HEX records: Data (00), EOF (01), Extended Segment (02),
 * Start Segment (03), Extended Linear (04), Start Linear (05).
 * Complement to s_record. Useful for bootloaders.
 * Pure C99, no heap, no OS.
 */

#ifndef INTEL_HEX_H
#define INTEL_HEX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INTEL_HEX_MAX_DATA 32

typedef enum {
    INTEL_HEX_OK           =  0,
    INTEL_HEX_ERR_PARAM    = -1,
    INTEL_HEX_ERR_FORMAT   = -2,
    INTEL_HEX_ERR_SHORT    = -3,
    INTEL_HEX_ERR_OVERFLOW = -4,
    INTEL_HEX_ERR_CHECKSUM = -5,
} intel_hex_err_t;

typedef enum {
    INTEL_HEX_DATA          = 0x00,
    INTEL_HEX_EOF           = 0x01,
    INTEL_HEX_EXT_SEGMENT   = 0x02,
    INTEL_HEX_START_SEGMENT = 0x03,
    INTEL_HEX_EXT_LINEAR    = 0x04,
    INTEL_HEX_START_LINEAR  = 0x05,
} intel_hex_type_t;

typedef struct {
    intel_hex_type_t type;
    uint16_t         address;
    uint8_t          data[INTEL_HEX_MAX_DATA];
    uint8_t          data_len;
} intel_hex_record_t;

/** Decode one Intel HEX line. Only CR and LF may follow the checksum. An
 *  undefined record type returns INTEL_HEX_ERR_FORMAT, and so does a byte count
 *  or address that does not fit the type: EOF holds 0 bytes, types 02 and 04
 *  hold 2 and types 03 and 05 hold 4, the last four at address 0.
 *  @thread_safe: yes @isr_safe: yes */
intel_hex_err_t intel_hex_decode(const char *line, size_t len,
                                 intel_hex_record_t *out);

/** Encode one Intel HEX record. Returns bytes written via out_len. An undefined
 *  record type returns INTEL_HEX_ERR_PARAM. A byte count or address that does
 *  not fit the type returns INTEL_HEX_ERR_FORMAT, by the rule intel_hex_decode
 *  gives. @thread_safe: yes @isr_safe: yes */
intel_hex_err_t intel_hex_encode(const intel_hex_record_t *rec,
                                 char *buf, size_t cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* INTEL_HEX_H */
