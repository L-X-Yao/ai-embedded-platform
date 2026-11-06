// SPDX-License-Identifier: MIT
/*
 * sha256 -- SHA-256 cryptographic hash (FIPS 180-4)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Streaming interface (init/update/final) plus one-shot convenience.
 * Thread safety: each context is independent; pure functions are reentrant.
 */

#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Digest size in bytes. */
#define SHA256_DIGEST_SIZE  32
/** Internal block size in bytes. */
#define SHA256_BLOCK_SIZE   64

/** Error codes. */
typedef enum {
    SHA256_OK        =  0,
    SHA256_ERR_PARAM = -1,
} sha256_err_t;

/** Streaming hash context. */
typedef struct {
    uint32_t state[8];          /**< Intermediate hash (H0..H7). */
    uint8_t  buffer[64];        /**< Partial block accumulator. */
    uint64_t count;             /**< Total bytes hashed so far. */
} sha256_t;

/**
 * Initialise a SHA-256 context to the standard IV.
 *
 * @param ctx  Caller-allocated context.  Must not be NULL.
 * @return     SHA256_OK on success, SHA256_ERR_PARAM on NULL.
 * @thread_safe: no
 */
sha256_err_t sha256_init(sha256_t *ctx);

/**
 * Feed arbitrary-length data into the hash.
 *
 * May be called zero or more times between init and final.
 *
 * @param ctx   Initialised context.  Must not be NULL.
 * @param data  Input bytes.  May be NULL only when len == 0.
 * @param len   Number of bytes.
 * @return      SHA256_OK on success, SHA256_ERR_PARAM on NULL ctx, or on NULL
 *              data with len > 0, which leaves the context unchanged.
 * @thread_safe: caller_must_lock
 */
sha256_err_t sha256_update(sha256_t *ctx, const uint8_t *data, size_t len);

/**
 * Finalise the hash and produce the 32-byte digest.
 *
 * After this call the context is invalidated; call sha256_init to reuse.
 *
 * @param ctx   Initialised context.  Must not be NULL.
 * @param hash  Output buffer (32 bytes).  Must not be NULL.
 * @return      SHA256_OK on success, SHA256_ERR_PARAM on NULL input.
 * @thread_safe: caller_must_lock
 */
sha256_err_t sha256_final(sha256_t *ctx, uint8_t hash[32]);

/**
 * One-shot convenience: hash a complete message.
 *
 * Equivalent to init + update + final.  If data breaks the rule below, hash
 * is set to all zeros.
 *
 * @param data  Input bytes.  May be NULL only when len == 0.
 * @param len   Number of bytes.
 * @param hash  Output buffer (32 bytes).  Must not be NULL.
 * @thread_safe: yes (uses only stack-local state) @isr_safe: yes
 */
void sha256_hash(const uint8_t *data, size_t len, uint8_t hash[32]);

#ifdef __cplusplus
}
#endif

#endif /* SHA256_H */
