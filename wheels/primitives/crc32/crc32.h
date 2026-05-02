// SPDX-License-Identifier: MIT
/*
 * crc32 — CRC32 (IEEE 802.3 / Ethernet / zlib-compatible), table-based
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Polynomial: 0xEDB88320 (reflected form of 0x04C11DB7)
 * Initial value: 0xFFFFFFFF, final XOR: 0xFFFFFFFF.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRC32_INIT  0xFFFFFFFFu

/** @thread_safe: yes (pure function, no state) @isr_safe: yes */
uint32_t crc32_compute(const uint8_t *data, size_t len);

/** Feed data into running CRC (pass CRC32_INIT for first call). @thread_safe: yes @isr_safe: yes */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);

/** Apply final XOR to produce the standard CRC32 value. @thread_safe: yes @isr_safe: yes */
uint32_t crc32_finalize(uint32_t crc);

#ifdef __cplusplus
}
#endif

#endif /* CRC32_H */
