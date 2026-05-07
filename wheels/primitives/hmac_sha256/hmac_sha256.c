// SPDX-License-Identifier: MIT
/*
 * hmac_sha256 -- HMAC-SHA256 message authentication (RFC 2104)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      sha256
 * Instances: multi
 */

#include "hmac_sha256.h"
#include "sha256.h"
#include <string.h>
#include <stdbool.h>

#define IPAD 0x36
#define OPAD 0x5c

typedef char assert_sha256_ctx_fits[(sizeof(sha256_ctx_t) <= 104) ? 1 : -1];

static void prepare_key(const uint8_t *key, size_t key_len,
                        uint8_t block[HMAC_SHA256_BLOCK_SIZE])
{
    memset(block, 0, HMAC_SHA256_BLOCK_SIZE);
    if (key_len > HMAC_SHA256_BLOCK_SIZE) {
        sha256_hash(key, key_len, block);
    } else {
        memcpy(block, key, key_len);
    }
}

hmac_sha256_err_t hmac_sha256_init(hmac_sha256_t *ctx,
                                   const uint8_t *key, size_t key_len)
{
    if (!ctx) { return HMAC_SHA256_ERR_PARAM; }
    if (!key && key_len > 0) { return HMAC_SHA256_ERR_PARAM; }

    uint8_t k_block[HMAC_SHA256_BLOCK_SIZE];
    prepare_key(key, key_len, k_block);

    uint8_t i_key_pad[HMAC_SHA256_BLOCK_SIZE];
    for (size_t i = 0; i < HMAC_SHA256_BLOCK_SIZE; i++) {
        i_key_pad[i]       = k_block[i] ^ IPAD;
        ctx->o_key_pad[i]  = k_block[i] ^ OPAD;
    }

    sha256_ctx_t *inner = (sha256_ctx_t *)ctx->i_ctx_buf;
    sha256_init(inner);
    sha256_update(inner, i_key_pad, HMAC_SHA256_BLOCK_SIZE);

    memset(k_block, 0, sizeof(k_block));
    memset(i_key_pad, 0, sizeof(i_key_pad));
    return HMAC_SHA256_OK;
}

hmac_sha256_err_t hmac_sha256_update(hmac_sha256_t *ctx,
                                     const uint8_t *data, size_t len)
{
    if (!ctx) { return HMAC_SHA256_ERR_PARAM; }
    if (!data && len > 0) { return HMAC_SHA256_ERR_PARAM; }

    sha256_ctx_t *inner = (sha256_ctx_t *)ctx->i_ctx_buf;
    sha256_update(inner, data, len);
    return HMAC_SHA256_OK;
}

hmac_sha256_err_t hmac_sha256_final(hmac_sha256_t *ctx, uint8_t mac[32])
{
    if (!ctx || !mac) { return HMAC_SHA256_ERR_PARAM; }

    uint8_t inner_hash[SHA256_DIGEST_SIZE];
    sha256_ctx_t *inner = (sha256_ctx_t *)ctx->i_ctx_buf;
    sha256_final(inner, inner_hash);

    sha256_ctx_t outer;
    sha256_init(&outer);
    sha256_update(&outer, ctx->o_key_pad, HMAC_SHA256_BLOCK_SIZE);
    sha256_update(&outer, inner_hash, SHA256_DIGEST_SIZE);
    sha256_final(&outer, mac);

    memset(inner_hash, 0, sizeof(inner_hash));
    memset(ctx, 0, sizeof(*ctx));
    return HMAC_SHA256_OK;
}

void hmac_sha256_compute(const uint8_t *key, size_t key_len,
                         const uint8_t *data, size_t data_len,
                         uint8_t mac[32])
{
    if (!mac) { return; }

    hmac_sha256_t ctx;
    (void)hmac_sha256_init(&ctx, key, key_len);
    (void)hmac_sha256_update(&ctx, data, data_len);
    (void)hmac_sha256_final(&ctx, mac);
}

bool hmac_sha256_verify(const uint8_t a[32], const uint8_t b[32])
{
    if (!a || !b) { return false; }

    volatile uint8_t diff = 0;
    for (size_t i = 0; i < HMAC_SHA256_DIGEST_SIZE; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}
