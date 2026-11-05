// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "base64.h"

/* ---- Encode tests ----------------------------------------------------- */

TEST(test_encode_empty)
{
    char buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_encode((const uint8_t *)"", 0, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 0);
    ASSERT_EQ(buf[0], '\0');
}

TEST(test_encode_f)
{
    char buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_encode((const uint8_t *)"f", 1, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 4);
    ASSERT_TRUE(strcmp(buf, "Zg==") == 0);
}

TEST(test_encode_fo)
{
    char buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_encode((const uint8_t *)"fo", 2, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 4);
    ASSERT_TRUE(strcmp(buf, "Zm8=") == 0);
}

TEST(test_encode_foo)
{
    char buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_encode((const uint8_t *)"foo", 3, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 4);
    ASSERT_TRUE(strcmp(buf, "Zm9v") == 0);
}

TEST(test_encode_foobar)
{
    char buf[16];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_encode((const uint8_t *)"foobar", 6, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 8);
    ASSERT_TRUE(strcmp(buf, "Zm9vYmFy") == 0);
}

/* ---- Decode tests (roundtrip) ----------------------------------------- */

TEST(test_decode_empty)
{
    uint8_t buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_decode("", 0, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 0);
}

TEST(test_decode_f)
{
    uint8_t buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_decode("Zg==", 4, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 1);
    ASSERT_EQ(buf[0], 'f');
}

TEST(test_decode_fo)
{
    uint8_t buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_decode("Zm8=", 4, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 2);
    ASSERT_TRUE(memcmp(buf, "fo", 2) == 0);
}

TEST(test_decode_foo)
{
    uint8_t buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_decode("Zm9v", 4, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 3);
    ASSERT_TRUE(memcmp(buf, "foo", 3) == 0);
}

TEST(test_decode_foobar)
{
    uint8_t buf[16];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_decode("Zm9vYmFy", 8, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 6);
    ASSERT_TRUE(memcmp(buf, "foobar", 6) == 0);
}

TEST(test_decode_without_padding)
{
    /* "Zg==" without padding → "Zg" */
    uint8_t buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_decode("Zg", 2, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 1);
    ASSERT_EQ(buf[0], 'f');

    /* "Zm8=" without padding → "Zm8" */
    len = sizeof(buf);
    ASSERT_EQ(base64_decode("Zm8", 3, buf, &len), BASE64_OK);
    ASSERT_EQ(len, 2);
    ASSERT_TRUE(memcmp(buf, "fo", 2) == 0);
}

/* ---- Binary roundtrip ------------------------------------------------- */

TEST(test_binary_roundtrip)
{
    /* Build a buffer with all byte values 0x00..0xFF */
    uint8_t input[256];
    for (int i = 0; i < 256; i++) {
        input[i] = (uint8_t)i;
    }

    char encoded[512];
    uint16_t enc_len = sizeof(encoded);
    ASSERT_EQ(base64_encode(input, 256, encoded, &enc_len), BASE64_OK);
    ASSERT_EQ(enc_len, base64_encoded_size(256));

    uint8_t decoded[300];
    uint16_t dec_len = sizeof(decoded);
    ASSERT_EQ(base64_decode(encoded, enc_len, decoded, &dec_len), BASE64_OK);
    ASSERT_EQ(dec_len, 256);
    ASSERT_TRUE(memcmp(input, decoded, 256) == 0);
}

/* ---- Error handling --------------------------------------------------- */

TEST(test_encode_overflow)
{
    char buf[4];   /* need 5 for "Zg==" + NUL */
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_encode((const uint8_t *)"f", 1, buf, &len),
              BASE64_ERR_OVERFLOW);
}

TEST(test_decode_overflow)
{
    uint8_t buf[2];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_decode("Zm9vYmFy", 8, buf, &len),
              BASE64_ERR_OVERFLOW);
}

TEST(test_decode_invalid_char)
{
    uint8_t buf[8];
    uint16_t len = sizeof(buf);
    ASSERT_EQ(base64_decode("Zg!!", 4, buf, &len), BASE64_ERR_INVALID);
}

TEST(test_null_pointers)
{
    uint16_t len = 16;
    char cbuf[16];
    uint8_t ubuf[16];

    /* Encode: NULL out */
    ASSERT_EQ(base64_encode((const uint8_t *)"x", 1, NULL, &len),
              BASE64_ERR_PARAM);
    /* Encode: NULL out_len */
    ASSERT_EQ(base64_encode((const uint8_t *)"x", 1, cbuf, NULL),
              BASE64_ERR_PARAM);
    /* Encode: NULL data with non-zero len */
    len = sizeof(cbuf);
    ASSERT_EQ(base64_encode(NULL, 5, cbuf, &len), BASE64_ERR_PARAM);

    /* Decode: NULL out */
    len = sizeof(ubuf);
    ASSERT_EQ(base64_decode("Zg==", 4, NULL, &len), BASE64_ERR_PARAM);
    /* Decode: NULL out_len */
    ASSERT_EQ(base64_decode("Zg==", 4, ubuf, NULL), BASE64_ERR_PARAM);
    /* Decode: NULL b64 with non-zero len */
    len = sizeof(ubuf);
    ASSERT_EQ(base64_decode(NULL, 4, ubuf, &len), BASE64_ERR_PARAM);
}

/* ---- Size calculation ------------------------------------------------- */

TEST(test_size_functions)
{
    ASSERT_EQ(base64_encoded_size(0), 0);
    ASSERT_EQ(base64_encoded_size(1), 4);
    ASSERT_EQ(base64_encoded_size(2), 4);
    ASSERT_EQ(base64_encoded_size(3), 4);
    ASSERT_EQ(base64_encoded_size(4), 8);
    ASSERT_EQ(base64_encoded_size(6), 8);
    ASSERT_EQ(base64_encoded_size(256), 344);

    /* decoded_size is an upper bound */
    ASSERT_TRUE(base64_decoded_size(4) >= 3);
    ASSERT_TRUE(base64_decoded_size(8) >= 6);
}

TEST(test_encode_length_past_uint16_limit)
{
    static const uint8_t data[49150];
    char out[16];
    uint16_t out_len = sizeof(out);

    /* 49150 bytes need 65536 characters, which wraps to 0 in uint16_t. */
    ASSERT_EQ(base64_encode(data, sizeof(data), out, &out_len), BASE64_ERR_OVERFLOW);
}

TEST(test_encode_longest_input)
{
    static const uint8_t data[49149];
    static char out[65533];
    uint16_t out_len = sizeof(out);

    ASSERT_EQ(base64_encode(data, sizeof(data), out, &out_len), BASE64_OK);
    ASSERT_EQ(out_len, 65532);
}

TEST_MAIN("base64")
    RUN(test_encode_empty);
    RUN(test_encode_f);
    RUN(test_encode_fo);
    RUN(test_encode_foo);
    RUN(test_encode_foobar);
    RUN(test_decode_empty);
    RUN(test_decode_f);
    RUN(test_decode_fo);
    RUN(test_decode_foo);
    RUN(test_decode_foobar);
    RUN(test_decode_without_padding);
    RUN(test_binary_roundtrip);
    RUN(test_encode_overflow);
    RUN(test_decode_overflow);
    RUN(test_decode_invalid_char);
    RUN(test_null_pointers);
    RUN(test_size_functions);
    RUN(test_encode_length_past_uint16_limit);
    RUN(test_encode_longest_input);
TEST_MAIN_END()
