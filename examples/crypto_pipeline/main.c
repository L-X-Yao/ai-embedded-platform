// SPDX-License-Identifier: MIT
// crypto_pipeline — hash, sign, encrypt, encode a sensor payload
//
// Wheels: sha256, hmac_sha256, aes128, base64, hex_codec

#include "sha256.h"
#include "hmac_sha256.h"
#include "aes128.h"
#include "base64.h"
#include "hex_codec.h"
#include <stdio.h>
#include <string.h>

static const uint8_t hmac_key[16] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
};

static const uint8_t aes_key[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

int main(void)
{
    const char *payload = "{\"temp\":23.5,\"hum\":61}";
    uint8_t hash[SHA256_DIGEST_SIZE];
    uint8_t mac[HMAC_SHA256_DIGEST_SIZE];
    uint8_t block_in[16];
    uint8_t block_enc[16];
    uint8_t block_dec[16];
    char    hex_buf[SHA256_DIGEST_SIZE * 2 + 1];
    char    b64_buf[128];
    uint16_t b64_len;

    printf("=== Crypto Pipeline Demo ===\n");
    printf("Payload: %s\n\n", payload);

    /* Step 1: SHA-256 hash */
    sha256_hash((const uint8_t *)payload, strlen(payload), hash);
    hex_encode(hash, SHA256_DIGEST_SIZE, hex_buf, sizeof(hex_buf));
    printf("[SHA-256]  %s\n", hex_buf);

    /* Step 2: HMAC-SHA256 signature */
    hmac_sha256_compute(hmac_key, sizeof(hmac_key),
                        (const uint8_t *)payload, strlen(payload), mac);
    hex_encode(mac, HMAC_SHA256_DIGEST_SIZE, hex_buf, sizeof(hex_buf));
    printf("[HMAC]     %s\n", hex_buf);

    /* Step 3: AES-128 encrypt first 16 bytes of payload */
    aes128_ctx_t aes;
    aes128_init(&aes, aes_key);

    memset(block_in, 0, sizeof(block_in));
    memcpy(block_in, payload,
           strlen(payload) < 16 ? strlen(payload) : 16);

    aes128_encrypt_block(&aes, block_in, block_enc);
    hex_encode(block_enc, 16, hex_buf, 33);
    printf("[AES-ENC]  %s\n", hex_buf);

    /* Step 4: Decrypt and verify round-trip */
    aes128_decrypt_block(&aes, block_enc, block_dec);
    aes128_clear(&aes);

    if (memcmp(block_in, block_dec, 16) == 0) {
        printf("[AES-DEC]  round-trip OK\n");
    } else {
        printf("[AES-DEC]  MISMATCH!\n");
        return 1;
    }

    /* Step 5: Base64-encode the encrypted block */
    b64_len = sizeof(b64_buf);
    base64_encode(block_enc, 16, b64_buf, &b64_len);
    printf("[BASE64]   %s\n", b64_buf);

    /* Step 6: Verify HMAC */
    uint8_t mac2[HMAC_SHA256_DIGEST_SIZE];
    hmac_sha256_compute(hmac_key, sizeof(hmac_key),
                        (const uint8_t *)payload, strlen(payload), mac2);
    if (hmac_sha256_verify(mac, mac2)) {
        printf("\nSignature verified OK\n");
    } else {
        printf("\nSignature MISMATCH!\n");
        return 1;
    }

    printf("=== PASSED ===\n");
    return 0;
}
