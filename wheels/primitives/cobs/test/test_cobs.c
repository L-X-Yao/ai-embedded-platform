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
TEST_MAIN_END()
