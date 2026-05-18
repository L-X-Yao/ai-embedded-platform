// SPDX-License-Identifier: MIT
/*
 * uuid — UUID v4 generation and parsing for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * RFC 4122 UUID v4 (random-based): 122 random bits + version/variant.
 * RNG callback injection — caller provides the random source.
 * No malloc, no global state.
 * Thread safety: all functions are reentrant; thread safety depends on
 *                the caller-provided RNG callback.
 */

#ifndef UUID_H
#define UUID_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UUID_OK         =  0,
    UUID_ERR_PARAM = -1,
    UUID_ERR_FORMAT = -2,
} uuid_err_t;

/** 128-bit UUID stored as 16 raw bytes (network byte order). */
typedef struct {
    uint8_t bytes[16];
} uuid_t;

/** Nil UUID constant (all zeros). */
extern const uuid_t UUID_NIL;

/**
 * Random number source callback.
 * The caller provides this to supply random bytes for UUID generation.
 *
 * @param ctx   Opaque context pointer (passed through from generate call).
 * @param buf   Buffer to fill with random bytes.
 * @param len   Number of random bytes requested.
 */
typedef void (*uuid_rng_t)(void *ctx, uint8_t *buf, uint16_t len);

/**
 * Generate a UUID v4 (random-based).
 *
 * Fills 16 bytes from the RNG callback, then sets version=4 and
 * variant=RFC 4122 bits.
 *
 * @param uuid  Output UUID. Must not be NULL.
 * @param rng   Random byte source callback. Must not be NULL.
 * @param ctx   Opaque context passed to rng. May be NULL.
 * @return      UUID_OK on success, UUID_ERR_PARAM if uuid or rng is NULL.
 * @thread_safe: yes if the provided rng callback is thread-safe @isr_safe: yes
 */
uuid_err_t uuid_v4_generate(uuid_t *uuid, uuid_rng_t rng, void *ctx);

/**
 * Format UUID to string: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx".
 *
 * @param uuid      Input UUID. Must not be NULL.
 * @param buf       Output buffer. Must not be NULL.
 * @param buf_size  Size of buf in bytes. Must be >= 37 (36 chars + NUL).
 * @return          UUID_OK on success, UUID_ERR_PARAM on invalid args.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uuid_err_t uuid_to_string(const uuid_t *uuid, char *buf, uint16_t buf_size);

/**
 * Parse UUID from string in 8-4-4-4-12 hex format.
 *
 * Accepts both uppercase and lowercase hex digits.
 *
 * @param uuid  Output UUID. Must not be NULL.
 * @param str   Input string. Must not be NULL. Must be exactly 36 chars.
 * @return      UUID_OK on success, UUID_ERR_PARAM if NULL,
 *              UUID_ERR_FORMAT if string is malformed.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uuid_err_t uuid_from_string(uuid_t *uuid, const char *str);

/**
 * Compare two UUIDs for equality.
 *
 * @param a  First UUID. Must not be NULL.
 * @param b  Second UUID. Must not be NULL.
 * @return   true if equal, false otherwise (including if either is NULL).
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
bool uuid_equal(const uuid_t *a, const uuid_t *b);

/**
 * Check if UUID is nil (all zeros).
 *
 * @param uuid  UUID to check. Must not be NULL.
 * @return      true if nil, false otherwise (including if NULL).
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
bool uuid_is_nil(const uuid_t *uuid);

#ifdef __cplusplus
}
#endif

#endif /* UUID_H */
