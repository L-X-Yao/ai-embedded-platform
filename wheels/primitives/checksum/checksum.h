/*
 * checksum — common checksum algorithms collection
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
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

/** @thread_safe: yes (pure function, no state) */
uint8_t checksum_xor(const uint8_t *data, size_t len);

uint8_t checksum_sum8(const uint8_t *data, size_t len);

uint16_t checksum_fletcher16(const uint8_t *data, size_t len);

uint8_t checksum_lrc(const uint8_t *data, size_t len);

uint16_t checksum_internet(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CHECKSUM_H */
