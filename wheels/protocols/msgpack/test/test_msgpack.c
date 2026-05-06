// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "msgpack.h"
#include <string.h>

/* ---- helper: encode then decode roundtrip ---- */

static uint8_t g_buf[256];

/* ---- encoder init / len ---- */

TEST(test_enc_init_null)
{
    msgpack_enc_t enc;
    ASSERT_EQ(msgpack_enc_init(NULL, g_buf, sizeof(g_buf)), MSGPACK_ERR_PARAM);
    ASSERT_EQ(msgpack_enc_init(&enc, NULL, 16), MSGPACK_ERR_PARAM);
}

TEST(test_enc_init_ok)
{
    msgpack_enc_t enc;
    ASSERT_EQ(msgpack_enc_init(&enc, g_buf, sizeof(g_buf)), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 0);
}

/* ---- nil / bool ---- */

TEST(test_nil_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_nil(&enc), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 1);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_NIL);
}

TEST(test_bool_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_bool(&enc, true), MSGPACK_OK);
    ASSERT_EQ(msgpack_enc_bool(&enc, false), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_BOOL);
    ASSERT_TRUE(tok.val.boolean);
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_BOOL);
    ASSERT_FALSE(tok.val.boolean);
}

/* ---- unsigned integers ---- */

TEST(test_uint_fixint)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_uint(&enc, 42), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 1);  /* fixint = 1 byte */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_UINT);
    ASSERT_EQ((long long)tok.val.u32, 42);
}

TEST(test_uint8_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_uint(&enc, 200), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 2);  /* uint8 = 0xcc + byte */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_UINT);
    ASSERT_EQ((long long)tok.val.u32, 200);
}

TEST(test_uint16_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_uint(&enc, 1000), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ((long long)tok.val.u32, 1000);
}

TEST(test_uint32_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_uint(&enc, 100000), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ((long long)tok.val.u32, 100000);
}

/* ---- signed integers ---- */

TEST(test_int_negative_fixint)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_int(&enc, -5), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 1);  /* neg fixint = 1 byte */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_INT);
    ASSERT_EQ(tok.val.i32, -5);
}

TEST(test_int16_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_int(&enc, -500), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_INT);
    ASSERT_EQ(tok.val.i32, -500);
}

/* ---- float ---- */

TEST(test_float_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_float(&enc, 3.14f), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 5);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_FLOAT);
    ASSERT_NEAR(tok.val.f32, 3.14f, 0.001f);
}

/* ---- string ---- */

TEST(test_str_fixstr)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_str(&enc, "hello", 5), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 6);  /* 1 hdr + 5 data */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_STR);
    ASSERT_EQ((long long)tok.val.str.len, 5);
    ASSERT_TRUE(memcmp(tok.val.str.ptr, "hello", 5) == 0);
}

/* ---- binary ---- */

TEST(test_bin_roundtrip)
{
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_bin(&enc, payload, sizeof(payload)), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_BIN);
    ASSERT_EQ((long long)tok.val.bin.len, 4);
    ASSERT_TRUE(memcmp(tok.val.bin.ptr, payload, 4) == 0);
}

/* ---- array / map ---- */

TEST(test_array_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_array(&enc, 3), MSGPACK_OK);
    ASSERT_EQ(msgpack_enc_uint(&enc, 1), MSGPACK_OK);
    ASSERT_EQ(msgpack_enc_uint(&enc, 2), MSGPACK_OK);
    ASSERT_EQ(msgpack_enc_uint(&enc, 3), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_ARRAY);
    ASSERT_EQ((long long)tok.val.array_count, 3);
    /* unpack 3 elements */
    for (int i = 1; i <= 3; i++) {
        ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
        ASSERT_EQ(tok.type, MSGPACK_TYPE_UINT);
        ASSERT_EQ((long long)tok.val.u32, i);
    }
}

TEST(test_map_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_map(&enc, 1), MSGPACK_OK);
    ASSERT_EQ(msgpack_enc_str(&enc, "temp", 4), MSGPACK_OK);
    ASSERT_EQ(msgpack_enc_float(&enc, 25.5f), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_MAP);
    ASSERT_EQ((long long)tok.val.map_count, 1);
    /* key */
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_STR);
    ASSERT_EQ((long long)tok.val.str.len, 4);
    /* value */
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_FLOAT);
    ASSERT_NEAR(tok.val.f32, 25.5f, 0.01f);
}

/* ---- error paths ---- */

TEST(test_enc_overflow)
{
    uint8_t tiny[1];
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, tiny, sizeof(tiny));
    ASSERT_EQ(msgpack_enc_nil(&enc), MSGPACK_OK);     /* uses 1 byte */
    ASSERT_EQ(msgpack_enc_nil(&enc), MSGPACK_ERR_OVERFLOW); /* no room */
}

TEST(test_dec_short)
{
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, 0);
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

TEST(test_dec_null_param)
{
    msgpack_token_t tok;
    ASSERT_EQ(msgpack_dec_next(NULL, &tok), MSGPACK_ERR_PARAM);
    msgpack_dec_t dec;
    msgpack_dec_init(&dec, g_buf, 1);
    ASSERT_EQ(msgpack_dec_next(&dec, NULL), MSGPACK_ERR_PARAM);
}

/* ---- positive int zero ---- */

TEST(test_uint_zero)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_uint(&enc, 0), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 1);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_UINT);
    ASSERT_EQ((long long)tok.val.u32, 0);
}

/* ---- positive int via signed path ---- */

TEST(test_int_positive_via_signed)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_int(&enc, 42), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_UINT);
    ASSERT_EQ((long long)tok.val.u32, 42);
}

/* ---- unsupported format byte triggers ERR_FORMAT ---- */

TEST(test_dec_format_float64)
{
    /* 0xcb = float64 — intentionally unsupported on MCU */
    const uint8_t data[] = {0xcb, 0x40, 0x09, 0x21, 0xfb,
                            0x54, 0x44, 0x2d, 0x18};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_FORMAT);
}

TEST(test_dec_format_fixext1)
{
    /* 0xd4 = fixext1 — extension types unsupported */
    const uint8_t data[] = {0xd4, 0x01, 0x00};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_FORMAT);
}

/* ---- int8 range: -33 to -128 ---- */

TEST(test_int8_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_int(&enc, -100), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 2); /* FMT_INT8 + 1 byte */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_INT);
    ASSERT_EQ(tok.val.i32, -100);
}

/* ---- int8 boundary: -33 (just outside fixint range) ---- */

TEST(test_int8_boundary)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_int(&enc, -33), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 2);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_INT);
    ASSERT_EQ(tok.val.i32, -33);
}

/* ---- int32 range: -32769 and below ---- */

TEST(test_int32_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_int(&enc, -70000), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 5); /* FMT_INT32 + 4 bytes */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_INT);
    ASSERT_EQ(tok.val.i32, -70000);
}

/* ---- int32 boundary: INT32_MIN ---- */

TEST(test_int32_min)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_int(&enc, -2147483647 - 1), MSGPACK_OK);

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_INT);
    ASSERT_EQ(tok.val.i32, -2147483647 - 1);
}

/* ---- str8: 32-255 bytes ---- */

TEST(test_str8_roundtrip)
{
    /* 50-byte string triggers str8 path */
    static const char s50[50] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWX";

    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_str(&enc, s50, 50), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 52); /* FMT_STR8 + 1 len byte + 50 */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_STR);
    ASSERT_EQ((long long)tok.val.str.len, 50);
}

/* ---- str16: 256-65535 bytes — use a 300-byte string ---- */

TEST(test_str16_roundtrip)
{
    static uint8_t bigbuf[700];
    static char s300[300];
    size_t i;
    for (i = 0; i < 300; i++) s300[i] = 'A';

    msgpack_enc_t enc;
    msgpack_enc_init(&enc, bigbuf, sizeof(bigbuf));
    ASSERT_EQ(msgpack_enc_str(&enc, s300, 300), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 303); /* FMT_STR16 + 2 + 300 */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, bigbuf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_STR);
    ASSERT_EQ((long long)tok.val.str.len, 300);
}

/* ---- bin16: 256-65535 bytes ---- */

TEST(test_bin16_roundtrip)
{
    static uint8_t bigbuf[700];
    static uint8_t payload[300];
    size_t i;
    for (i = 0; i < 300; i++) payload[i] = (uint8_t)(i & 0xFF);

    msgpack_enc_t enc;
    msgpack_enc_init(&enc, bigbuf, sizeof(bigbuf));
    ASSERT_EQ(msgpack_enc_bin(&enc, payload, 300), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 303); /* FMT_BIN16 + 2 + 300 */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, bigbuf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_BIN);
    ASSERT_EQ((long long)tok.val.bin.len, 300);
}

/* ---- array16: 16-65535 elements ---- */

TEST(test_array16_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_array(&enc, 20), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 3); /* FMT_ARRAY16 + 2 */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_ARRAY);
    ASSERT_EQ((long long)tok.val.array_count, 20);
}

/* ---- array32: > 65535 elements ---- */

TEST(test_array32_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_array(&enc, 70000), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 5); /* FMT_ARRAY32 + 4 */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_ARRAY);
    ASSERT_EQ((long long)tok.val.array_count, 70000);
}

/* ---- map16: 16-65535 pairs ---- */

TEST(test_map16_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_map(&enc, 20), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 3); /* FMT_MAP16 + 2 */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_MAP);
    ASSERT_EQ((long long)tok.val.map_count, 20);
}

/* ---- map32: > 65535 pairs ---- */

TEST(test_map32_roundtrip)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_map(&enc, 70000), MSGPACK_OK);
    ASSERT_EQ((long long)msgpack_enc_len(&enc), 5); /* FMT_MAP32 + 4 */

    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, g_buf, msgpack_enc_len(&enc));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_OK);
    ASSERT_EQ(tok.type, MSGPACK_TYPE_MAP);
    ASSERT_EQ((long long)tok.val.map_count, 70000);
}

/* ---- overflow on small buffers for new paths ---- */

TEST(test_int8_overflow)
{
    uint8_t tiny[1];
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, tiny, sizeof(tiny));
    /* -100 needs 2 bytes (int8 path) but only 1 byte available */
    ASSERT_EQ(msgpack_enc_int(&enc, -100), MSGPACK_ERR_OVERFLOW);
}

TEST(test_int32_overflow)
{
    uint8_t tiny[4];
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, tiny, sizeof(tiny));
    /* -70000 needs 5 bytes (int32 path) but only 4 available */
    ASSERT_EQ(msgpack_enc_int(&enc, -70000), MSGPACK_ERR_OVERFLOW);
}

TEST(test_str8_overflow)
{
    /* Buffer too small for a 50-char str8 */
    uint8_t tiny[10];
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, tiny, sizeof(tiny));
    ASSERT_EQ(msgpack_enc_str(&enc, "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789012345678901234", 50),
              MSGPACK_ERR_OVERFLOW);
}

TEST(test_str_null_nonempty)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    /* NULL ptr with non-zero len is a param error */
    ASSERT_EQ(msgpack_enc_str(&enc, NULL, 5), MSGPACK_ERR_PARAM);
}

TEST(test_bin_null_nonempty)
{
    msgpack_enc_t enc;
    msgpack_enc_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(msgpack_enc_bin(&enc, NULL, 5), MSGPACK_ERR_PARAM);
}

/* ---- dec_init with NULL buf and zero len is OK ---- */

TEST(test_dec_init_null_zero)
{
    msgpack_dec_t dec;
    ASSERT_EQ(msgpack_dec_init(&dec, NULL, 0), MSGPACK_OK);
}

/* ---- dec_init NULL ptr with non-zero len ---- */

TEST(test_dec_init_null_nonzero)
{
    msgpack_dec_t dec;
    ASSERT_EQ(msgpack_dec_init(&dec, NULL, 5), MSGPACK_ERR_PARAM);
}

/* ---- msgpack_enc_len on NULL returns 0 ---- */

TEST(test_enc_len_null)
{
    ASSERT_EQ((long long)msgpack_enc_len(NULL), 0);
}

/* ---- truncated int8 decode ---- */

TEST(test_dec_int8_short)
{
    /* FMT_INT8 header with no payload byte */
    const uint8_t data[] = {0xd0};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated int32 decode ---- */

TEST(test_dec_int32_short)
{
    /* FMT_INT32 header with only 2 payload bytes instead of 4 */
    const uint8_t data[] = {0xd2, 0x00, 0x01};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated str8 decode ---- */

TEST(test_dec_str8_short_header)
{
    /* FMT_STR8 header only — missing length byte */
    const uint8_t data[] = {0xd9};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

TEST(test_dec_str8_short_payload)
{
    /* FMT_STR8 with len=5 but only 2 payload bytes */
    const uint8_t data[] = {0xd9, 0x05, 'A', 'B'};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated str16 decode ---- */

TEST(test_dec_str16_short)
{
    /* FMT_STR16 with only 1 length byte instead of 2 */
    const uint8_t data[] = {0xda, 0x01};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated str32 decode ---- */

TEST(test_dec_str32_short)
{
    /* FMT_STR32 with only 3 length bytes instead of 4 */
    const uint8_t data[] = {0xdb, 0x00, 0x00, 0x01};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated bin16 decode ---- */

TEST(test_dec_bin16_short)
{
    /* FMT_BIN16 with only 1 length byte */
    const uint8_t data[] = {0xc5, 0x01};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated bin32 decode ---- */

TEST(test_dec_bin32_short)
{
    /* FMT_BIN32 with only 3 length bytes */
    const uint8_t data[] = {0xc6, 0x00, 0x00, 0x01};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated array16 decode ---- */

TEST(test_dec_array16_short)
{
    const uint8_t data[] = {0xdc, 0x00};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated array32 decode ---- */

TEST(test_dec_array32_short)
{
    const uint8_t data[] = {0xdd, 0x00, 0x00, 0x01};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated map16 decode ---- */

TEST(test_dec_map16_short)
{
    const uint8_t data[] = {0xde, 0x00};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated map32 decode ---- */

TEST(test_dec_map32_short)
{
    const uint8_t data[] = {0xdf, 0x00, 0x00, 0x01};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- fixstr truncated payload ---- */

TEST(test_dec_fixstr_short_payload)
{
    /* fixstr with len=5 but only 2 bytes of payload follow */
    const uint8_t data[] = {0xa5, 'A', 'B'};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- truncated uint16 / uint32 decode ---- */

TEST(test_dec_uint16_short)
{
    const uint8_t data[] = {0xcd, 0x01};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

TEST(test_dec_uint32_short)
{
    const uint8_t data[] = {0xce, 0x00, 0x01, 0x00};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- bin8 truncated payload ---- */

TEST(test_dec_bin8_short_payload)
{
    /* FMT_BIN8 with len=4 but only 2 bytes of payload */
    const uint8_t data[] = {0xc4, 0x04, 0xDE, 0xAD};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

/* ---- float32 truncated ---- */

TEST(test_dec_float32_short)
{
    const uint8_t data[] = {0xca, 0x40, 0x48};
    msgpack_dec_t dec;
    msgpack_token_t tok;
    msgpack_dec_init(&dec, data, sizeof(data));
    ASSERT_EQ(msgpack_dec_next(&dec, &tok), MSGPACK_ERR_SHORT);
}

TEST_MAIN("msgpack")
    RUN(test_enc_init_null);
    RUN(test_enc_init_ok);
    RUN(test_nil_roundtrip);
    RUN(test_bool_roundtrip);
    RUN(test_uint_fixint);
    RUN(test_uint8_roundtrip);
    RUN(test_uint16_roundtrip);
    RUN(test_uint32_roundtrip);
    RUN(test_int_negative_fixint);
    RUN(test_int16_roundtrip);
    RUN(test_float_roundtrip);
    RUN(test_str_fixstr);
    RUN(test_bin_roundtrip);
    RUN(test_array_roundtrip);
    RUN(test_map_roundtrip);
    RUN(test_enc_overflow);
    RUN(test_dec_short);
    RUN(test_dec_null_param);
    RUN(test_uint_zero);
    RUN(test_int_positive_via_signed);
    RUN(test_dec_format_float64);
    RUN(test_dec_format_fixext1);
    RUN(test_int8_roundtrip);
    RUN(test_int8_boundary);
    RUN(test_int32_roundtrip);
    RUN(test_int32_min);
    RUN(test_str8_roundtrip);
    RUN(test_str16_roundtrip);
    RUN(test_bin16_roundtrip);
    RUN(test_array16_roundtrip);
    RUN(test_array32_roundtrip);
    RUN(test_map16_roundtrip);
    RUN(test_map32_roundtrip);
    RUN(test_int8_overflow);
    RUN(test_int32_overflow);
    RUN(test_str8_overflow);
    RUN(test_str_null_nonempty);
    RUN(test_bin_null_nonempty);
    RUN(test_dec_init_null_zero);
    RUN(test_dec_init_null_nonzero);
    RUN(test_enc_len_null);
    RUN(test_dec_int8_short);
    RUN(test_dec_int32_short);
    RUN(test_dec_str8_short_header);
    RUN(test_dec_str8_short_payload);
    RUN(test_dec_str16_short);
    RUN(test_dec_str32_short);
    RUN(test_dec_bin16_short);
    RUN(test_dec_bin32_short);
    RUN(test_dec_array16_short);
    RUN(test_dec_array32_short);
    RUN(test_dec_map16_short);
    RUN(test_dec_map32_short);
    RUN(test_dec_fixstr_short_payload);
    RUN(test_dec_uint16_short);
    RUN(test_dec_uint32_short);
    RUN(test_dec_bin8_short_payload);
    RUN(test_dec_float32_short);
TEST_MAIN_END()
