// SPDX-License-Identifier: MIT
/*
 * aes128 -- AES-128 ECB block encrypt/decrypt
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Standard AES-128 per FIPS 197.  ECB only (single 16-byte block).
 * Higher modes (CBC, CTR, GCM) are built on top by the caller.
 */

#ifndef AES128_H
#define AES128_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Error codes. */
typedef enum {
    AES128_OK        =  0,
    AES128_ERR_PARAM = -1,
} aes128_err_t;

/** Expanded round-key schedule (176 bytes = 11 round keys x 16 bytes). */
typedef struct {
    uint8_t round_keys[176];
} aes128_ctx_t;

/**
 * Expand a 128-bit key into the round-key schedule.
 *
 * @param ctx  Caller-allocated context.  Must not be NULL.
 * @param key  128-bit (16-byte) key.  Must not be NULL.
 * @return     AES128_OK on success, AES128_ERR_PARAM on NULL input.
 * @thread_safe: no
 */
aes128_err_t aes128_init(aes128_ctx_t *ctx, const uint8_t key[16]);

/**
 * Encrypt a single 128-bit block (ECB).
 *
 * @param ctx  Initialised context (from aes128_init).  Must not be NULL.
 * @param in   16-byte plaintext block.  Must not be NULL.
 * @param out  16-byte ciphertext output.  Must not be NULL.  May alias in.
 * @return     AES128_OK on success, AES128_ERR_PARAM on NULL input.
 * @thread_safe: yes (read-only access to ctx) @isr_safe: yes
 */
aes128_err_t aes128_encrypt_block(const aes128_ctx_t *ctx,
                                  const uint8_t in[16],
                                  uint8_t out[16]);

/**
 * Decrypt a single 128-bit block (ECB).
 *
 * @param ctx  Initialised context (from aes128_init).  Must not be NULL.
 * @param in   16-byte ciphertext block.  Must not be NULL.
 * @param out  16-byte plaintext output.  Must not be NULL.  May alias in.
 * @return     AES128_OK on success, AES128_ERR_PARAM on NULL input.
 * @thread_safe: yes (read-only access to ctx) @isr_safe: yes
 */
aes128_err_t aes128_decrypt_block(const aes128_ctx_t *ctx,
                                  const uint8_t in[16],
                                  uint8_t out[16]);

/**
 * Zeroize the round-key schedule to erase key material from memory.
 *
 * Uses a volatile write loop so the compiler cannot optimize it away.
 * Call this when the key is no longer needed.
 *
 * @param ctx  Context to clear.  Must not be NULL.
 * @return     AES128_OK on success, AES128_ERR_PARAM on NULL input.
 * @thread_safe: no
 */
aes128_err_t aes128_clear(aes128_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AES128_H */
