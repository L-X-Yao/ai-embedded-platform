// SPDX-License-Identifier: MIT
/*
 * hash_fnv — FNV-1a hash function (32-bit and 64-bit)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Simple, fast, no-table hash for hash maps, checksums, string hashing.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef HASH_FNV_H
#define HASH_FNV_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FNV-1a 32-bit constants */
#define FNV1A_32_OFFSET_BASIS  0x811C9DC5u
#define FNV1A_32_PRIME         0x01000193u

/* FNV-1a 64-bit constants */
#define FNV1A_64_OFFSET_BASIS  0xCBF29CE484222325ULL
#define FNV1A_64_PRIME         0x00000100000001B3ULL

/**
 * Compute FNV-1a 32-bit hash over a byte buffer.
 *
 * @param data  Input buffer. May be NULL only when len == 0.
 * @param len   Number of bytes. 0 returns FNV1A_32_OFFSET_BASIS.
 * @return      32-bit hash value.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uint32_t fnv1a_32(const uint8_t *data, size_t len);

/**
 * Compute FNV-1a 64-bit hash over a byte buffer.
 *
 * @param data  Input buffer. May be NULL only when len == 0.
 * @param len   Number of bytes. 0 returns FNV1A_64_OFFSET_BASIS.
 * @return      64-bit hash value.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uint64_t fnv1a_64(const uint8_t *data, size_t len);

/**
 * Compute FNV-1a 32-bit hash over a NUL-terminated string.
 *
 * @param str   Input string. NULL returns FNV1A_32_OFFSET_BASIS.
 * @return      32-bit hash value (NUL terminator not included).
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uint32_t fnv1a_32_string(const char *str);

/**
 * Compute FNV-1a 64-bit hash over a NUL-terminated string.
 *
 * @param str   Input string. NULL returns FNV1A_64_OFFSET_BASIS.
 * @return      64-bit hash value (NUL terminator not included).
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uint64_t fnv1a_64_string(const char *str);

/**
 * Incremental FNV-1a 32-bit: absorb additional bytes into running hash.
 *
 * @param hash  Current hash (FNV1A_32_OFFSET_BASIS on first call).
 * @param data  Input buffer. May be NULL only when len == 0.
 * @param len   Number of bytes to absorb.
 * @return      Updated hash value.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uint32_t fnv1a_32_update(uint32_t hash, const uint8_t *data, size_t len);

/**
 * Incremental FNV-1a 64-bit: absorb additional bytes into running hash.
 *
 * @param hash  Current hash (FNV1A_64_OFFSET_BASIS on first call).
 * @param data  Input buffer. May be NULL only when len == 0.
 * @param len   Number of bytes to absorb.
 * @return      Updated hash value.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uint64_t fnv1a_64_update(uint64_t hash, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HASH_FNV_H */
