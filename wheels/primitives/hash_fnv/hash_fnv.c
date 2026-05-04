// SPDX-License-Identifier: MIT
/*
 * hash_fnv — FNV-1a hash function (32-bit and 64-bit)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Algorithm: for each byte, hash ^= byte; hash *= prime.
 * Reference: http://www.isthe.com/chongo/tech/comp/fnv/
 */

#include "hash_fnv.h"
#include "contract.h"

uint32_t fnv1a_32_update(uint32_t hash, const uint8_t *data, size_t len)
{
    if (!data) return hash;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint32_t)data[i];
        hash *= FNV1A_32_PRIME;
    }
    return hash;
}

uint64_t fnv1a_64_update(uint64_t hash, const uint8_t *data, size_t len)
{
    if (!data) return hash;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)data[i];
        hash *= FNV1A_64_PRIME;
    }
    return hash;
}

uint32_t fnv1a_32(const uint8_t *data, size_t len)
{
    return fnv1a_32_update(FNV1A_32_OFFSET_BASIS, data, len);
}

uint64_t fnv1a_64(const uint8_t *data, size_t len)
{
    return fnv1a_64_update(FNV1A_64_OFFSET_BASIS, data, len);
}

uint32_t fnv1a_32_string(const char *str)
{
    if (!str) return FNV1A_32_OFFSET_BASIS;
    uint32_t hash = FNV1A_32_OFFSET_BASIS;
    while (*str) {
        hash ^= (uint32_t)(uint8_t)*str++;
        hash *= FNV1A_32_PRIME;
    }
    return hash;
}

uint64_t fnv1a_64_string(const char *str)
{
    if (!str) return FNV1A_64_OFFSET_BASIS;
    uint64_t hash = FNV1A_64_OFFSET_BASIS;
    while (*str) {
        hash ^= (uint64_t)(uint8_t)*str++;
        hash *= FNV1A_64_PRIME;
    }
    return hash;
}
