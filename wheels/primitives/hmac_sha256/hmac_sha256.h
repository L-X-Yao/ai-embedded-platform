// SPDX-License-Identifier: MIT
/*
 * hmac_sha256 -- HMAC-SHA256 message authentication (RFC 2104)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      sha256
 * Instances: multi
 *
 * Streaming interface (init/update/final) plus one-shot convenience.
 * Thread safety: each context is independent; pure functions are reentrant.
 */

#ifndef HMAC_SHA256_H
#define HMAC_SHA256_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HMAC_SHA256_DIGEST_SIZE  32
#define HMAC_SHA256_BLOCK_SIZE   64

typedef enum {
    HMAC_SHA256_OK        =  0,
    HMAC_SHA256_ERR_PARAM = -1,
} hmac_sha256_err_t;

typedef struct {
    uint8_t o_key_pad[HMAC_SHA256_BLOCK_SIZE]; /* internal: outer padded key */
    sha256_t inner;                            /* internal: inner hash; sets the alignment */
} hmac_sha256_t;

/**
 * Begin an HMAC-SHA256 computation with the given key.
 *
 * @param ctx      Caller-allocated context. Must not be NULL.
 * @param key      Key bytes. May be NULL only when key_len == 0.
 * @param key_len  Key length in bytes. Keys > 64 bytes are hashed first.
 * @return         HMAC_SHA256_OK on success.
 * @thread_safe: no @isr_safe: no
 */
hmac_sha256_err_t hmac_sha256_init(hmac_sha256_t *ctx,
                                   const uint8_t *key, size_t key_len);

/**
 * Feed data into the HMAC computation.
 *
 * @param ctx   Initialised context. Must not be NULL.
 * @param data  Input bytes. May be NULL only when len == 0.
 * @param len   Number of bytes.
 * @return      HMAC_SHA256_OK on success.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
hmac_sha256_err_t hmac_sha256_update(hmac_sha256_t *ctx,
                                     const uint8_t *data, size_t len);

/**
 * Finalise and produce the 32-byte MAC.
 *
 * After this call the context is invalidated; call init to reuse.
 *
 * @param ctx   Initialised context. Must not be NULL.
 * @param mac   Output buffer (32 bytes). Must not be NULL.
 * @return      HMAC_SHA256_OK on success.
 * @thread_safe: caller_must_lock @isr_safe: no
 */
hmac_sha256_err_t hmac_sha256_final(hmac_sha256_t *ctx, uint8_t mac[32]);

/**
 * One-shot convenience: compute HMAC-SHA256 of a complete message.
 * If key or data breaks the rule below, mac is set to all zeros.
 *
 * @param key      Key bytes. May be NULL only when key_len == 0.
 * @param key_len  Key length.
 * @param data     Input bytes. May be NULL only when data_len == 0.
 * @param data_len Number of bytes.
 * @param mac      Output buffer (32 bytes). Must not be NULL.
 * @thread_safe: yes @isr_safe: yes
 */
void hmac_sha256_compute(const uint8_t *key, size_t key_len,
                         const uint8_t *data, size_t data_len,
                         uint8_t mac[32]);

/**
 * Constant-time comparison of two MACs.
 *
 * @param a  First MAC (32 bytes). Must not be NULL.
 * @param b  Second MAC (32 bytes). Must not be NULL.
 * @return   true if equal.
 * @thread_safe: yes @isr_safe: yes
 */
bool hmac_sha256_verify(const uint8_t a[32], const uint8_t b[32]);

#ifdef __cplusplus
}
#endif

#endif /* HMAC_SHA256_H */
