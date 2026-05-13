// SPDX-License-Identifier: MIT
/*
 * prop_varint.c — property-based tests for varint (LEB128 + ZigZag)
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_varint.c ../varint.c -o prop_varint
 */

#include "test_harness.h"
#include "property_test.h"
#include "../varint.h"
#include <string.h>

/* Helper: arbitrary 64-bit unsigned from PRNG */
static uint64_t arbitrary_u64(uint32_t *seed_ptr)
{
    uint64_t hi = prop__xorshift32(seed_ptr);
    uint64_t lo = prop__xorshift32(seed_ptr);
    return (hi << 32) | lo;
}

/* Property: encode_u64 then decode_u64 returns original value */
PROPERTY(u64_roundtrip, 10000) {
    uint64_t value = arbitrary_u64(&seed);
    uint8_t buf[10];
    size_t enc_len = 0;

    PROP_ASSERT_EQ(
        varint_encode_u64(value, buf, sizeof(buf), &enc_len),
        VARINT_OK);
    PROP_ASSERT(enc_len >= 1 && enc_len <= 10);

    uint64_t decoded = 0;
    size_t dec_len = 0;
    PROP_ASSERT_EQ(
        varint_decode_u64(buf, enc_len, &decoded, &dec_len),
        VARINT_OK);
    PROP_ASSERT(decoded == value);
    PROP_ASSERT_EQ(dec_len, enc_len);
}

/* Property: encode_s64 then decode_s64 returns original signed value */
PROPERTY(s64_roundtrip, 10000) {
    int64_t value = (int64_t)arbitrary_u64(&seed);
    uint8_t buf[10];
    size_t enc_len = 0;

    PROP_ASSERT_EQ(
        varint_encode_s64(value, buf, sizeof(buf), &enc_len),
        VARINT_OK);

    int64_t decoded = 0;
    size_t dec_len = 0;
    PROP_ASSERT_EQ(
        varint_decode_s64(buf, enc_len, &decoded, &dec_len),
        VARINT_OK);
    PROP_ASSERT(decoded == value);
    PROP_ASSERT_EQ(dec_len, enc_len);
}

/* Property: encoded_size_u64 matches actual encode length */
PROPERTY(encoded_size_matches, 10000) {
    uint64_t value = arbitrary_u64(&seed);
    size_t predicted = varint_encoded_size_u64(value);

    uint8_t buf[10];
    size_t actual = 0;
    PROP_ASSERT_EQ(
        varint_encode_u64(value, buf, sizeof(buf), &actual),
        VARINT_OK);
    PROP_ASSERT_EQ(predicted, actual);
}

/* Property: small values (< 128) encode to exactly one byte */
PROPERTY(small_values_one_byte, 5000) {
    uint64_t value = (uint64_t)ARBI_RANGE(seed, 0, 127);
    uint8_t buf[10];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(
        varint_encode_u64(value, buf, sizeof(buf), &enc_len),
        VARINT_OK);
    PROP_ASSERT_EQ(enc_len, 1);
    PROP_ASSERT_EQ((uint64_t)buf[0], value);
}

/* Property: all bytes except the final one have continuation bit set */
PROPERTY(continuation_bits_correct, 10000) {
    uint64_t value = arbitrary_u64(&seed);
    uint8_t buf[10];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(
        varint_encode_u64(value, buf, sizeof(buf), &enc_len),
        VARINT_OK);
    for (size_t i = 0; i < enc_len - 1; i++) {
        PROP_ASSERT((buf[i] & 0x80u) != 0);  /* continuation bit set */
    }
    PROP_ASSERT((buf[enc_len - 1] & 0x80u) == 0);  /* terminator bit clear */
}

/* Property: tiny buffer cap returns ERR_OVERFLOW for multi-byte values */
PROPERTY(too_small_cap_rejected, 5000) {
    uint64_t value = (uint64_t)ARBI_RANGE(seed, 128, 65535);
    uint8_t buf[1];
    size_t enc_len = 0;
    PROP_ASSERT_EQ(
        varint_encode_u64(value, buf, sizeof(buf), &enc_len),
        VARINT_ERR_OVERFLOW);
}

/* Property: feeding arbitrary bytes into decoder never crashes */
PROPERTY(decode_no_crash, 10000) {
    uint8_t garbage[16];
    size_t len = ARBI_SIZE(seed, 16);
    ARBI_FILL(seed, garbage, len);

    uint64_t value = 0;
    size_t out_len = 0;
    (void)varint_decode_u64(garbage, len, &value, &out_len);

    int64_t svalue = 0;
    (void)varint_decode_s64(garbage, len, &svalue, &out_len);
    PROP_ASSERT(1);
}

TEST_MAIN("varint properties") {
    RUN(u64_roundtrip);
    RUN(s64_roundtrip);
    RUN(encoded_size_matches);
    RUN(small_values_one_byte);
    RUN(continuation_bits_correct);
    RUN(too_small_cap_rejected);
    RUN(decode_no_crash);
}
TEST_MAIN_END()
