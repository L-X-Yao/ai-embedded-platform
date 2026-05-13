// SPDX-License-Identifier: MIT
/*
 * prop_utf8.c — property-based tests for utf8
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_utf8.c ../utf8.c -o prop_utf8
 */

#include "test_harness.h"
#include "property_test.h"
#include "../utf8.h"
#include <string.h>

/* Helper: generate a random valid Unicode scalar value (skips surrogates). */
static uint32_t arbitrary_codepoint(uint32_t *seed_ptr)
{
    uint32_t cp = prop__xorshift32(seed_ptr) % 0x110000u;
    /* Surrogate half = U+D800..U+DFFF is invalid in UTF-8 */
    if (cp >= 0xD800u && cp <= 0xDFFFu) {
        cp = (cp - 0xD800u) | 0xE000u;  /* shift into BMP private use */
    }
    return cp;
}

/* Property: encode then decode returns the same codepoint */
PROPERTY(encode_decode_roundtrip, 10000) {
    uint32_t cp = arbitrary_codepoint(&seed);
    uint8_t buf[4];
    size_t enc_len = 0;

    utf8_err_t err = utf8_encode(cp, buf, sizeof(buf), &enc_len);
    PROP_ASSERT_EQ(err, UTF8_OK);
    PROP_ASSERT(enc_len >= 1 && enc_len <= 4);

    uint32_t decoded = 0;
    size_t dec_len = 0;
    err = utf8_decode(buf, enc_len, &decoded, &dec_len);
    PROP_ASSERT_EQ(err, UTF8_OK);
    PROP_ASSERT_EQ(decoded, cp);
    PROP_ASSERT_EQ(dec_len, enc_len);
}

/* Property: utf8_byte_length of lead byte matches encoded length */
PROPERTY(byte_length_matches_encoded, 10000) {
    uint32_t cp = arbitrary_codepoint(&seed);
    uint8_t buf[4];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(utf8_encode(cp, buf, sizeof(buf), &enc_len), UTF8_OK);
    PROP_ASSERT_EQ(utf8_byte_length(buf[0]), (uint8_t)enc_len);
}

/* Property: validate accepts any encoded codepoint */
PROPERTY(validate_accepts_encoded, 10000) {
    uint32_t cp = arbitrary_codepoint(&seed);
    uint8_t buf[4];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(utf8_encode(cp, buf, sizeof(buf), &enc_len), UTF8_OK);
    PROP_ASSERT_EQ(utf8_validate(buf, enc_len), UTF8_OK);
}

/* Property: codepoint_count of N encoded codepoints equals N */
PROPERTY(codepoint_count_matches, 2000) {
    /* generate a string of K codepoints, each 1-4 bytes, total <= 256 bytes */
    uint8_t buf[256];
    size_t total = 0;
    int n_codepoints = ARBI_RANGE(seed, 0, 30);
    int written = 0;

    for (int i = 0; i < n_codepoints; i++) {
        uint32_t cp = arbitrary_codepoint(&seed);
        size_t enc_len = 0;
        if (utf8_encode(cp, buf + total, sizeof(buf) - total, &enc_len) != UTF8_OK)
            break;
        total += enc_len;
        written++;
    }

    int32_t count = utf8_codepoint_count(buf, total);
    PROP_ASSERT_EQ(count, written);
}

/* Property: surrogate codepoints rejected by encoder */
PROPERTY(surrogates_rejected, 5000) {
    uint32_t cp = 0xD800u + (prop__xorshift32(&seed) & 0x7FFu);
    uint8_t buf[4];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(
        utf8_encode(cp, buf, sizeof(buf), &enc_len),
        UTF8_ERR_INVALID);
}

/* Property: feeding arbitrary bytes into validate/decode never crashes */
PROPERTY(decode_no_crash, 10000) {
    uint8_t garbage[64];
    size_t len = ARBI_SIZE(seed, 64);
    ARBI_FILL(seed, garbage, len);

    uint32_t cp = 0;
    size_t out_len = 0;
    (void)utf8_decode(garbage, len, &cp, &out_len);
    (void)utf8_validate(garbage, len);
    (void)utf8_codepoint_count(garbage, len);
    PROP_ASSERT(1);  /* reached without crash */
}

TEST_MAIN("utf8 properties") {
    RUN(encode_decode_roundtrip);
    RUN(byte_length_matches_encoded);
    RUN(validate_accepts_encoded);
    RUN(codepoint_count_matches);
    RUN(surrogates_rejected);
    RUN(decode_no_crash);
}
TEST_MAIN_END()
