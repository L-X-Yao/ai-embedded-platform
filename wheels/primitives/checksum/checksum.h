// SPDX-License-Identifier: MIT
/*
 * checksum — common checksum algorithms collection
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Provides XOR, BSD, Fletcher-16, and Internet (IP/TCP/UDP) checksums.
 * All pure / reentrant.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- ACSL formal specifications (Frama-C WP) ---------- */

/** @thread_safe: yes (pure function, no state) @isr_safe: yes */
/*@ requires len == 0 || \valid_read(data + (0 .. len - 1));
    assigns \nothing;
*/
uint8_t checksum_xor(const uint8_t *data, size_t len);

/** Compute 8-bit arithmetic sum of data bytes. @thread_safe: yes @isr_safe: yes */
/*@ requires len == 0 || \valid_read(data + (0 .. len - 1));
    assigns \nothing;
*/
uint8_t checksum_sum8(const uint8_t *data, size_t len);

/** Compute Fletcher-16 checksum. @thread_safe: yes @isr_safe: yes */
/*@ requires len == 0 || \valid_read(data + (0 .. len - 1));
    assigns \nothing;
    ensures \result <= 0xFFFF;
*/
uint16_t checksum_fletcher16(const uint8_t *data, size_t len);

/** Compute longitudinal redundancy check (two's complement of sum). @thread_safe: yes @isr_safe: yes */
/*@ requires len == 0 || \valid_read(data + (0 .. len - 1));
    assigns \nothing;
*/
uint8_t checksum_lrc(const uint8_t *data, size_t len);

/** Compute RFC 1071 Internet checksum (IP/TCP/UDP). @thread_safe: yes @isr_safe: yes */
/*@ requires len == 0 || \valid_read(data + (0 .. len - 1));
    assigns \nothing;
    ensures \result <= 0xFFFF;
*/
uint16_t checksum_internet(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CHECKSUM_H */
