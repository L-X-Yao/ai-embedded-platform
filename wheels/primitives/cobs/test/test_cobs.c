// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "cobs.h"

/* ── helpers ─────────────────────────────────────────────────────── */

static void encode_ok(const uint8_t *data, uint16_t len,
                      uint8_t *out, uint16_t *out_len)
{
    cobs_err_t err = cobs_encode(data, len, out, out_len);
    ASSERT_EQ(err, COBS_OK);
}

static void roundtrip(const uint8_t *data, uint16_t len)
{
    uint8_t enc[512];
    uint16_t enc_len = (uint16_t)sizeof(enc);
    cobs_err_t err = cobs_encode(data, len, enc, &enc_len);
    ASSERT_EQ(err, COBS_OK);

    /* verify no 0x00 in encoded output */
    for (uint16_t i = 0; i < enc_len; i++) {
        ASSERT_TRUE(enc[i] != 0x00);
    }

    uint8_t dec[512];
    uint16_t dec_len = (uint16_t)sizeof(dec);
    err = cobs_decode(enc, enc_len, dec, &dec_len);
    ASSERT_EQ(err, COBS_OK);
    ASSERT_EQ(dec_len, len);
    if (len > 0) {
        ASSERT_TRUE(memcmp(data, dec, len) == 0);
    }
}

/* ── encode tests ────────────────────────────────────────────────── */

TEST(test_encode_empty)
{
    uint8_t out[8];
    uint16_t out_len = (uint16_t)sizeof(out);
    encode_ok(NULL, 0, out, &out_len);
    /* empty input -> single overhead byte: 0x01 */
    ASSERT_EQ(out_len, 1);
    ASSERT_EQ_HEX(out[0], 0x01);
}

TEST(test_encode_single_nonzero)
{
    uint8_t data[] = {0x42};
    uint8_t out[8];
    uint16_t out_len = (uint16_t)sizeof(out);
    encode_ok(data, 1, out, &out_len);
    /* [0x02, 0x42] */
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ_HEX(out[0], 0x02);
    ASSERT_EQ_HEX(out[1], 0x42);
}

TEST(test_encode_single_zero)
{
    uint8_t data[] = {0x00};
    uint8_t out[8];
    uint16_t out_len = (uint16_t)sizeof(out);
    encode_ok(data, 1, out, &out_len);
    /* [0x01, 0x01] */
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ_HEX(out[0], 0x01);
    ASSERT_EQ_HEX(out[1], 0x01);
}

TEST(test_encode_hello)
{
    /* "Hello" has no zeros */
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o'};
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    encode_ok(data, 5, out, &out_len);
    /* [0x06, 'H', 'e', 'l', 'l', 'o'] */
    ASSERT_EQ(out_len, 6);
    ASSERT_EQ_HEX(out[0], 0x06);
    ASSERT_TRUE(memcmp(out + 1, data, 5) == 0);
}

TEST(test_encode_zeros_at_positions)
{
    /* data with zeros at start, middle, end */
    uint8_t data[] = {0x00, 0x11, 0x00, 0x22, 0x00};
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    encode_ok(data, 5, out, &out_len);
    /* [0x01, 0x02, 0x11, 0x02, 0x22, 0x01] */
    ASSERT_EQ(out_len, 6);
    uint8_t expected[] = {0x01, 0x02, 0x11, 0x02, 0x22, 0x01};
    ASSERT_TRUE(memcmp(out, expected, 6) == 0);
}

TEST(test_encode_254_nonzero)
{
    /* 254 non-zero bytes -> forces max block code 0xFF, then trailing 0x01 */
    uint8_t data[254];
    memset(data, 0xAA, 254);
    uint8_t out[512];
    uint16_t out_len = (uint16_t)sizeof(out);
    encode_ok(data, 254, out, &out_len);
    /* [0xFF, <254 bytes>, 0x01] = 256 bytes total */
    ASSERT_EQ(out_len, 256);
    ASSERT_EQ_HEX(out[0], 0xFF);
    ASSERT_EQ_HEX(out[255], 0x01);
}

TEST(test_encode_255_nonzero)
{
    /* 255 non-zero bytes -> two blocks */
    uint8_t data[255];
    memset(data, 0xBB, 255);
    uint8_t out[512];
    uint16_t out_len = (uint16_t)sizeof(out);
    encode_ok(data, 255, out, &out_len);
    /* [0xFF, <254 bytes>, 0x02, <1 byte>] = 257 bytes */
    ASSERT_EQ(out_len, 257);
    ASSERT_EQ_HEX(out[0], 0xFF);
    ASSERT_EQ_HEX(out[255], 0x02);
}

/* ── roundtrip tests ─────────────────────────────────────────────── */

TEST(test_roundtrip_empty)
{
    roundtrip(NULL, 0);
}

TEST(test_roundtrip_with_zeros)
{
    uint8_t data[] = {0x00, 0x11, 0x00, 0x22, 0x33, 0x00};
    roundtrip(data, (uint16_t)sizeof(data));
}

TEST(test_roundtrip_254_nonzero)
{
    uint8_t data[254];
    memset(data, 0xCC, 254);
    roundtrip(data, 254);
}

TEST(test_roundtrip_all_byte_values)
{
    /* all 256 byte values (0x00-0xFF) */
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = (uint8_t)i;
    }
    roundtrip(data, 256);
}

/* ── error handling ──────────────────────────────────────────────── */

TEST(test_null_params_encode)
{
    uint8_t buf[8];
    uint16_t len = (uint16_t)sizeof(buf);
    uint8_t data = 0x42;

    ASSERT_EQ(cobs_encode(&data, 1, NULL, &len), COBS_ERR_PARAM);
    len = (uint16_t)sizeof(buf);
    ASSERT_EQ(cobs_encode(&data, 1, buf, NULL), COBS_ERR_PARAM);
    len = (uint16_t)sizeof(buf);
    ASSERT_EQ(cobs_encode(NULL, 5, buf, &len), COBS_ERR_PARAM);
}

TEST(test_null_params_decode)
{
    uint8_t buf[8];
    uint16_t len = (uint16_t)sizeof(buf);
    uint8_t enc = 0x01;

    ASSERT_EQ(cobs_decode(&enc, 1, NULL, &len), COBS_ERR_PARAM);
    len = (uint16_t)sizeof(buf);
    ASSERT_EQ(cobs_decode(&enc, 1, buf, NULL), COBS_ERR_PARAM);
    len = (uint16_t)sizeof(buf);
    ASSERT_EQ(cobs_decode(NULL, 5, buf, &len), COBS_ERR_PARAM);
}

TEST(test_encode_buffer_too_small)
{
    uint8_t data[] = {0x11, 0x22, 0x33};
    uint8_t out[2];
    uint16_t out_len = (uint16_t)sizeof(out);
    ASSERT_EQ(cobs_encode(data, 3, out, &out_len), COBS_ERR_OVERFLOW);
}

TEST(test_decode_buffer_too_small)
{
    /* first encode something */
    uint8_t data[] = {0x11, 0x22, 0x33};
    uint8_t enc[16];
    uint16_t enc_len = (uint16_t)sizeof(enc);
    encode_ok(data, 3, enc, &enc_len);

    /* decode into too-small buffer */
    uint8_t out[1];
    uint16_t out_len = (uint16_t)sizeof(out);
    ASSERT_EQ(cobs_decode(enc, enc_len, out, &out_len), COBS_ERR_OVERFLOW);
}

TEST(test_decode_invalid_zero_in_stream)
{
    /* a zero byte in the encoded stream is invalid */
    uint8_t bad[] = {0x03, 0x11, 0x00, 0x22};
    uint8_t out[8];
    uint16_t out_len = (uint16_t)sizeof(out);
    ASSERT_EQ(cobs_decode(bad, 4, out, &out_len), COBS_ERR_INVALID);
}

TEST(test_decode_invalid_code_overrun)
{
    /* code byte says 5 data bytes follow, but only 2 remain */
    uint8_t bad[] = {0x05, 0x11, 0x22};
    uint8_t out[8];
    uint16_t out_len = (uint16_t)sizeof(out);
    ASSERT_EQ(cobs_decode(bad, 3, out, &out_len), COBS_ERR_INVALID);
}

/* ── size calculation ────────────────────────────────────────────── */

TEST(test_max_encoded_size)
{
    ASSERT_EQ(cobs_max_encoded_size(0), 1);
    ASSERT_EQ(cobs_max_encoded_size(1), 2);
    ASSERT_EQ(cobs_max_encoded_size(254), 256);
    ASSERT_EQ(cobs_max_encoded_size(255), 257);
    ASSERT_EQ(cobs_max_encoded_size(508), 511);
}

/* ── Mutation-hardening tests ─────────────────────────────────── */

/*
 * cobs_max_encoded_size: verify the formula data_len + data_len/254 + 1
 * at specific boundary values that differentiate floor(n/254) steps.
 * Catches /254 vs /255, + 1 vs + 0, and overflow guard mutations.
 */
TEST(test_max_encoded_size_formula)
{
    /* 0 bytes: 0 + 0/254 + 1 = 1 */
    ASSERT_EQ(cobs_max_encoded_size(0), 1);
    /* 1 byte: 1 + 0 + 1 = 2 */
    ASSERT_EQ(cobs_max_encoded_size(1), 2);
    /* 253 bytes: 253 + 0 + 1 = 254 */
    ASSERT_EQ(cobs_max_encoded_size(253), 254);
    /* 254 bytes: 254 + 1 + 1 = 256 (crosses the 254-boundary) */
    ASSERT_EQ(cobs_max_encoded_size(254), 256);
    /* 255 bytes: 255 + 1 + 1 = 257 */
    ASSERT_EQ(cobs_max_encoded_size(255), 257);
    /* 507 bytes: 507 + 1 + 1 = 509 */
    ASSERT_EQ(cobs_max_encoded_size(507), 509);
    /* 508 bytes: 508 + 2 + 1 = 511 (crosses second 254-boundary) */
    ASSERT_EQ(cobs_max_encoded_size(508), 511);
    /* Overflow guard: values above 65279 return UINT16_MAX */
    ASSERT_EQ(cobs_max_encoded_size(65280), (uint16_t)65535u);
    ASSERT_EQ(cobs_max_encoded_size(65535), (uint16_t)65535u);
}

/*
 * Exact encoded output: verify every byte of simple patterns —
 * catches write++ vs write-- and code++ vs code-- mutations.
 */
TEST(test_encode_two_nonzero_bytes_exact)
{
    uint8_t data[] = {0x11, 0x22};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_encode(data, 2, out, &out_len), COBS_OK);
    /* [0x03, 0x11, 0x22] = 3 bytes */
    ASSERT_EQ(out_len, 3);
    ASSERT_EQ_HEX(out[0], 0x03);
    ASSERT_EQ_HEX(out[1], 0x11);
    ASSERT_EQ_HEX(out[2], 0x22);
}

TEST(test_encode_two_zeros_exact)
{
    uint8_t data[] = {0x00, 0x00};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_encode(data, 2, out, &out_len), COBS_OK);
    /* [0x01, 0x01, 0x01] = 3 bytes */
    ASSERT_EQ(out_len, 3);
    ASSERT_EQ_HEX(out[0], 0x01);
    ASSERT_EQ_HEX(out[1], 0x01);
    ASSERT_EQ_HEX(out[2], 0x01);
}

TEST(test_encode_nonzero_zero_nonzero_exact)
{
    /* [0xAA, 0x00, 0xBB]: split by the zero */
    uint8_t data[] = {0xAA, 0x00, 0xBB};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_encode(data, 3, out, &out_len), COBS_OK);
    /* [0x02, 0xAA, 0x02, 0xBB] = 4 bytes */
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ_HEX(out[0], 0x02);
    ASSERT_EQ_HEX(out[1], 0xAA);
    ASSERT_EQ_HEX(out[2], 0x02);
    ASSERT_EQ_HEX(out[3], 0xBB);
}

/*
 * Block boundary at exactly 254 non-zero bytes: code byte must be 0xFF
 * and the next code byte must be 0x01 (empty trailing block) —
 * catches code == COBS_BLOCK_MAX vs code > COBS_BLOCK_MAX mutations.
 */
TEST(test_encode_254_boundary_exact_bytes)
{
    uint8_t data[254];
    uint8_t out[512];
    memset(data, 0x55, 254);
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_encode(data, 254, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 256); /* [0xFF, 254 bytes, 0x01] */
    ASSERT_EQ_HEX(out[0], 0xFF);
    for (int i = 1; i <= 254; i++) {
        ASSERT_EQ_HEX(out[i], 0x55);
    }
    ASSERT_EQ_HEX(out[255], 0x01); /* final code: empty block */
}

/*
 * Block boundary at 255 non-zero bytes: first block has 254 bytes (code=0xFF),
 * second block has 1 byte (code=0x02) —
 * catches off-by-one in block splitting.
 */
TEST(test_encode_255_boundary_exact_bytes)
{
    uint8_t data[255];
    uint8_t out[512];
    memset(data, 0x77, 255);
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_encode(data, 255, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 257); /* [0xFF, 254 bytes, 0x02, 1 byte] */
    ASSERT_EQ_HEX(out[0], 0xFF);
    ASSERT_EQ_HEX(out[255], 0x02);
    ASSERT_EQ_HEX(out[256], 0x77);
}

/*
 * Decode exact output: verify byte values, not just length.
 * Catches out[write]=0 vs out[write]=1, and write++ vs write-- mutations.
 */
TEST(test_decode_exact_single_zero)
{
    /* [0x01, 0x01] → [0x00] */
    uint8_t enc[] = {0x01, 0x01};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_decode(enc, 2, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 1);
    ASSERT_EQ_HEX(out[0], 0x00);
}

TEST(test_decode_exact_single_nonzero)
{
    /* [0x02, 0x42] → [0x42] */
    uint8_t enc[] = {0x02, 0x42};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_decode(enc, 2, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 1);
    ASSERT_EQ_HEX(out[0], 0x42);
}

TEST(test_decode_exact_zero_sequence)
{
    /* [0x01, 0x01, 0x01] → [0x00, 0x00] */
    uint8_t enc[] = {0x01, 0x01, 0x01};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_decode(enc, 3, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ_HEX(out[0], 0x00);
    ASSERT_EQ_HEX(out[1], 0x00);
}

TEST(test_decode_exact_mixed)
{
    /* [0x02, 0xAA, 0x02, 0xBB] → [0xAA, 0x00, 0xBB] */
    uint8_t enc[] = {0x02, 0xAA, 0x02, 0xBB};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_decode(enc, 4, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 3);
    ASSERT_EQ_HEX(out[0], 0xAA);
    ASSERT_EQ_HEX(out[1], 0x00);
    ASSERT_EQ_HEX(out[2], 0xBB);
}

/*
 * Implicit zero is NOT emitted after the final block (code < 0xFF but i >= enc_len) —
 * verify no spurious trailing zero gets added.
 * Catches the i < enc_len guard condition in the "emit implicit zero" path.
 */
TEST(test_decode_no_trailing_implicit_zero)
{
    /* [0x03, 0x11, 0x22] → [0x11, 0x22] (no trailing zero) */
    uint8_t enc[] = {0x03, 0x11, 0x22};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_decode(enc, 3, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ_HEX(out[0], 0x11);
    ASSERT_EQ_HEX(out[1], 0x22);
}

/*
 * Decode: code=0xFF means a full 254-byte block with NO implicit zero after it —
 * catches the code < COBS_BLOCK_MAX vs code <= COBS_BLOCK_MAX mutation.
 */
TEST(test_decode_full_block_no_implicit_zero)
{
    /* Encode 254 non-zero bytes, then decode and verify decoded length == 254 */
    uint8_t data[254];
    memset(data, 0xAA, 254);
    uint8_t enc[512];
    uint16_t enc_len = sizeof(enc);
    ASSERT_EQ(cobs_encode(data, 254, enc, &enc_len), COBS_OK);
    ASSERT_EQ(enc_len, 256); /* sanity */

    uint8_t dec[512];
    uint16_t dec_len = sizeof(dec);
    ASSERT_EQ(cobs_decode(enc, enc_len, dec, &dec_len), COBS_OK);
    /* Must be exactly 254 bytes — no extra zero inserted */
    ASSERT_EQ(dec_len, 254);
    for (int i = 0; i < 254; i++) {
        ASSERT_EQ_HEX(dec[i], 0xAA);
    }
}

/*
 * cobs_encode output_len is updated on success —
 * catches *out_len = write vs *out_len = write + 1 mutations.
 */
TEST(test_encode_output_len_updated)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t out[16];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_encode(data, 3, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 4); /* [0x04, 0x01, 0x02, 0x03] */
}

/*
 * cobs_decode output_len is updated on success —
 * catches *out_len = write vs *out_len = write - 1 mutations.
 */
TEST(test_decode_output_len_updated)
{
    uint8_t enc[] = {0x04, 0x01, 0x02, 0x03};
    uint8_t out[16];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_decode(enc, 4, out, &out_len), COBS_OK);
    ASSERT_EQ(out_len, 3);
}

/*
 * cobs_encode with capacity == max_size - 1 must return COBS_ERR_OVERFLOW,
 * while capacity == max_size must succeed —
 * catches < vs <= in the capacity check.
 */
TEST(test_encode_capacity_at_exact_boundary)
{
    uint8_t data[] = {0x11, 0x22, 0x33}; /* max_size = 4+1 = 5, no wait: 3+0+1=4 */
    uint16_t max_sz = cobs_max_encoded_size(3); /* = 4 */
    ASSERT_EQ(max_sz, 4);

    uint8_t out[8];
    uint16_t cap;

    /* capacity == max_size: must succeed */
    cap = max_sz;
    ASSERT_EQ(cobs_encode(data, 3, out, &cap), COBS_OK);

    /* capacity == max_size - 1: must fail */
    cap = (uint16_t)(max_sz - 1u);
    ASSERT_EQ(cobs_encode(data, 3, out, &cap), COBS_ERR_OVERFLOW);
}

/*
 * cobs_decode: code byte == 0x00 is invalid — verify COBS_ERR_INVALID specifically.
 */
TEST(test_decode_leading_zero_code_exact_error)
{
    uint8_t bad[] = {0x00};
    uint8_t out[8];
    uint16_t out_len = sizeof(out);
    ASSERT_EQ(cobs_decode(bad, 1, out, &out_len), COBS_ERR_INVALID);
}

/*
 * Roundtrip: trailing zero at end of payload is correctly preserved —
 * catches the implicit-zero emission logic edge case.
 */
TEST(test_roundtrip_trailing_zero)
{
    uint8_t data[] = {0x11, 0x22, 0x00};
    roundtrip(data, sizeof(data));
}

TEST(test_roundtrip_leading_zero)
{
    uint8_t data[] = {0x00, 0x11, 0x22};
    roundtrip(data, sizeof(data));
}

TEST(test_roundtrip_all_zeros)
{
    uint8_t data[8] = {0};
    roundtrip(data, sizeof(data));
}

TEST(test_roundtrip_single_nonzero)
{
    uint8_t data[] = {0xFF};
    roundtrip(data, sizeof(data));
}

TEST_MAIN("cobs")
    RUN(test_encode_empty);
    RUN(test_encode_single_nonzero);
    RUN(test_encode_single_zero);
    RUN(test_encode_hello);
    RUN(test_encode_zeros_at_positions);
    RUN(test_encode_254_nonzero);
    RUN(test_encode_255_nonzero);
    RUN(test_roundtrip_empty);
    RUN(test_roundtrip_with_zeros);
    RUN(test_roundtrip_254_nonzero);
    RUN(test_roundtrip_all_byte_values);
    RUN(test_null_params_encode);
    RUN(test_null_params_decode);
    RUN(test_encode_buffer_too_small);
    RUN(test_decode_buffer_too_small);
    RUN(test_decode_invalid_zero_in_stream);
    RUN(test_decode_invalid_code_overrun);
    RUN(test_max_encoded_size);
    RUN(test_max_encoded_size_formula);
    RUN(test_encode_two_nonzero_bytes_exact);
    RUN(test_encode_two_zeros_exact);
    RUN(test_encode_nonzero_zero_nonzero_exact);
    RUN(test_encode_254_boundary_exact_bytes);
    RUN(test_encode_255_boundary_exact_bytes);
    RUN(test_decode_exact_single_zero);
    RUN(test_decode_exact_single_nonzero);
    RUN(test_decode_exact_zero_sequence);
    RUN(test_decode_exact_mixed);
    RUN(test_decode_no_trailing_implicit_zero);
    RUN(test_decode_full_block_no_implicit_zero);
    RUN(test_encode_output_len_updated);
    RUN(test_decode_output_len_updated);
    RUN(test_encode_capacity_at_exact_boundary);
    RUN(test_decode_leading_zero_code_exact_error);
    RUN(test_roundtrip_trailing_zero);
    RUN(test_roundtrip_leading_zero);
    RUN(test_roundtrip_all_zeros);
    RUN(test_roundtrip_single_nonzero);
TEST_MAIN_END()
