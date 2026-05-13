// SPDX-License-Identifier: MIT
/*
 * bcd — binary-coded decimal encode/decode for RTC chips and legacy protocols
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: stateless
 *
 * Each byte encodes two decimal digits (0-9) in its upper and lower nibble.
 * Common in DS3231, PCF8563, and other RTC register formats.
 * Pure C99, no heap, no OS, fully reentrant.
 */

#ifndef BCD_H
#define BCD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BCD_OK          =  0,
    BCD_ERR_PARAM   = -1,
    BCD_ERR_RANGE   = -2,
    BCD_ERR_INVALID = -3,
} bcd_err_t;

/** Check if a BCD byte is valid (both nibbles 0-9). @thread_safe: yes @isr_safe: yes */
bool bcd_is_valid(uint8_t bcd);

/** BCD byte to binary (0x59 -> 59). Undefined if !bcd_is_valid(bcd). @thread_safe: yes @isr_safe: yes */
uint8_t bcd_to_bin(uint8_t bcd);

/** Binary to BCD byte (59 -> 0x59). Undefined if bin > 99. @thread_safe: yes @isr_safe: yes */
uint8_t bcd_from_bin(uint8_t bin);

/** Validated BCD-to-binary. @thread_safe: yes @isr_safe: yes */
bcd_err_t bcd_to_bin_safe(uint8_t bcd, uint8_t *out);

/** Validated binary-to-BCD. @thread_safe: yes @isr_safe: yes */
bcd_err_t bcd_from_bin_safe(uint8_t bin, uint8_t *out);

/** Batch BCD-to-binary. @thread_safe: yes @isr_safe: yes */
bcd_err_t bcd_buf_to_bin(const uint8_t *bcd, uint8_t *bin, uint16_t len);

/** Batch binary-to-BCD. @thread_safe: yes @isr_safe: yes */
bcd_err_t bcd_buf_from_bin(const uint8_t *bin, uint8_t *bcd, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* BCD_H */
