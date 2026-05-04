// SPDX-License-Identifier: MIT
/*
 * sha256 -- SHA-256 cryptographic hash (FIPS 180-4)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Byte-oriented, portable to 8/16/32-bit MCUs.  No dynamic allocation.
 * Reference algorithm: FIPS 180-4 (2015), Section 6.2.
 */

#include "sha256.h"
#include "contract.h"
#include <string.h>

/* ---- SHA-256 round constants (Section 4.2.2) ---- */

static const uint32_t K[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

/* ---- Bit operations (Section 3.2, 4.1.2) ---- */

#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIGMA0(x)    (ROTR(x,  2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIGMA1(x)    (ROTR(x,  6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define sigma0(x)    (ROTR(x,  7) ^ ROTR(x, 18) ^ ((x) >>  3))
#define sigma1(x)    (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

/* ---- Big-endian helpers ---- */

static uint32_t be32_load(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void be32_store(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
}

/* ---- Compress one 512-bit block (Section 6.2.2) ---- */

static void sha256_compress(uint32_t state[8], const uint8_t block[64])
{
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;

    /* Prepare message schedule. */
    for (int t = 0; t < 16; t++) {
        W[t] = be32_load(&block[t * 4]);
    }
    for (int t = 16; t < 64; t++) {
        W[t] = sigma1(W[t - 2]) + W[t - 7] + sigma0(W[t - 15]) + W[t - 16];
    }

    /* Initialise working variables. */
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    /* 64 rounds. */
    for (int t = 0; t < 64; t++) {
        uint32_t T1 = h + SIGMA1(e) + CH(e, f, g) + K[t] + W[t];
        uint32_t T2 = SIGMA0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }

    /* Update hash values. */
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

/* ---- Public API ---- */

sha256_err_t sha256_init(sha256_ctx_t *ctx)
{
    if (!ctx) return SHA256_ERR_PARAM;

    /* Initial hash value (FIPS 180-4, Section 5.3.3). */
    ctx->state[0] = 0x6A09E667;
    ctx->state[1] = 0xBB67AE85;
    ctx->state[2] = 0x3C6EF372;
    ctx->state[3] = 0xA54FF53A;
    ctx->state[4] = 0x510E527F;
    ctx->state[5] = 0x9B05688C;
    ctx->state[6] = 0x1F83D9AB;
    ctx->state[7] = 0x5BE0CD19;
    ctx->count = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));

    return SHA256_OK;
}

sha256_err_t sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx) return SHA256_ERR_PARAM;
    if (len == 0 || !data) return SHA256_OK;

    size_t buffered = (size_t)(ctx->count & 63u);

    ctx->count += (uint64_t)len;

    /* Fill partial block if any. */
    if (buffered > 0) {
        size_t space = 64 - buffered;
        size_t copy = (len < space) ? len : space;
        memcpy(&ctx->buffer[buffered], data, copy);
        data += copy;
        len -= copy;
        buffered += copy;
        if (buffered == 64) {
            sha256_compress(ctx->state, ctx->buffer);
            buffered = 0;
        }
    }

    /* Process full blocks directly from input. */
    while (len >= 64) {
        sha256_compress(ctx->state, data);
        data += 64;
        len -= 64;
    }

    /* Buffer remaining bytes. */
    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }

    return SHA256_OK;
}

sha256_err_t sha256_final(sha256_ctx_t *ctx, uint8_t hash[32])
{
    if (!ctx || !hash) return SHA256_ERR_PARAM;

    uint64_t total_bits = ctx->count * 8;
    size_t buffered = (size_t)(ctx->count & 63u);

    /* Append 0x80 padding byte. */
    ctx->buffer[buffered++] = 0x80;

    /* If not enough room for the 8-byte length, pad and compress. */
    if (buffered > 56) {
        memset(&ctx->buffer[buffered], 0, 64 - buffered);
        sha256_compress(ctx->state, ctx->buffer);
        buffered = 0;
    }

    /* Pad to 56 bytes, then append 64-bit big-endian bit count. */
    memset(&ctx->buffer[buffered], 0, 56 - buffered);
    ctx->buffer[56] = (uint8_t)(total_bits >> 56);
    ctx->buffer[57] = (uint8_t)(total_bits >> 48);
    ctx->buffer[58] = (uint8_t)(total_bits >> 40);
    ctx->buffer[59] = (uint8_t)(total_bits >> 32);
    ctx->buffer[60] = (uint8_t)(total_bits >> 24);
    ctx->buffer[61] = (uint8_t)(total_bits >> 16);
    ctx->buffer[62] = (uint8_t)(total_bits >>  8);
    ctx->buffer[63] = (uint8_t)(total_bits);

    sha256_compress(ctx->state, ctx->buffer);

    /* Produce final hash in big-endian. */
    for (int i = 0; i < 8; i++) {
        be32_store(&hash[i * 4], ctx->state[i]);
    }

    /* Wipe sensitive state. */
    memset(ctx, 0, sizeof(*ctx));

    return SHA256_OK;
}

void sha256_hash(const uint8_t *data, size_t len, uint8_t hash[32])
{
    sha256_ctx_t ctx;
    (void)sha256_init(&ctx);
    (void)sha256_update(&ctx, data, len);
    (void)sha256_final(&ctx, hash);
}
