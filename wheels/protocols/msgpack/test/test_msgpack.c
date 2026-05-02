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
TEST_MAIN_END()
