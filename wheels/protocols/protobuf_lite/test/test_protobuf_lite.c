#include "test_harness.h"
#include "protobuf_lite.h"
#include <string.h>
#include <limits.h>
#include <float.h>

/* ---- Helper: encode then decode roundtrip ---- */
static uint8_t g_buf[256];

/* ---- Varint tests ---- */

TEST(test_varint_small)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_varint(&enc, 1, 42), PB_OK);
    /* tag=0x08 (field 1, varint), value=0x2A */
    ASSERT_EQ(pb_encoder_len(&enc), 2);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.field_num, 1);
    ASSERT_EQ(f.wire_type, PB_WIRE_VARINT);
    ASSERT_EQ(f.value.varint, 42);
    ASSERT_TRUE(pb_decoder_done(&dec));
}

TEST(test_varint_medium)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_varint(&enc, 2, 300), PB_OK);
    /* 300 = 0x012C, varint needs 2 bytes */
    ASSERT_EQ(pb_encoder_len(&enc), 3);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.field_num, 2);
    ASSERT_EQ(f.value.varint, 300);
}

TEST(test_varint_large)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    uint64_t big = 0xFFFFFFFFULL;
    ASSERT_EQ(pb_encode_varint(&enc, 3, big), PB_OK);
    /* 0xFFFFFFFF needs 5 varint bytes + 1 tag = 6 */
    ASSERT_EQ(pb_encoder_len(&enc), 6);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.value.varint, big);
}

/* ---- Sint32 / ZigZag tests ---- */

TEST(test_sint32_positive)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_sint32(&enc, 1, 100), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(pb_zigzag_decode32((uint32_t)f.value.varint), 100);
}

TEST(test_sint32_negative)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_sint32(&enc, 1, -50), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(pb_zigzag_decode32((uint32_t)f.value.varint), -50);
}

TEST(test_sint32_minus_one)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_sint32(&enc, 1, -1), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    /* ZigZag(-1) = 1 */
    ASSERT_EQ(f.value.varint, 1);
    ASSERT_EQ(pb_zigzag_decode32((uint32_t)f.value.varint), -1);
}

TEST(test_sint32_min)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_sint32(&enc, 1, INT32_MIN), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(pb_zigzag_decode32((uint32_t)f.value.varint), INT32_MIN);
}

/* ---- Fixed32 test ---- */

TEST(test_fixed32)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_fixed32(&enc, 4, 0xDEADBEEF), PB_OK);
    /* tag (1 byte) + 4 bytes = 5 */
    ASSERT_EQ(pb_encoder_len(&enc), 5);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.wire_type, PB_WIRE_32BIT);
    ASSERT_EQ_HEX(f.value.fixed32, 0xDEADBEEF);
}

/* ---- Float tests ---- */

TEST(test_float_zero)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_float(&enc, 5, 0.0f), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    float result;
    memcpy(&result, &f.value.fixed32, sizeof(result));
    ASSERT_NEAR(result, 0.0f, 0.001f);
}

TEST(test_float_positive)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_float(&enc, 5, 1.5f), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    float result;
    memcpy(&result, &f.value.fixed32, sizeof(result));
    ASSERT_NEAR(result, 1.5f, 0.001f);
}

TEST(test_float_negative)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_float(&enc, 5, -3.14f), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    float result;
    memcpy(&result, &f.value.fixed32, sizeof(result));
    ASSERT_NEAR(result, -3.14f, 0.01f);
}

/* ---- String test ---- */

TEST(test_string)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_string(&enc, 6, "hello"), PB_OK);
    /* tag(1) + length varint(1) + 5 chars = 7 */
    ASSERT_EQ(pb_encoder_len(&enc), 7);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.wire_type, PB_WIRE_LENGTH);
    ASSERT_EQ(f.value.bytes.len, 5);
    ASSERT_MEMEQ(f.value.bytes.data, "hello");
}

/* ---- Bytes test (binary data) ---- */

TEST(test_bytes)
{
    uint8_t payload[] = {0x00, 0xFF, 0x42, 0x80};
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_bytes(&enc, 7, payload, sizeof(payload)), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.value.bytes.len, 4);
    ASSERT_EQ(f.value.bytes.data[0], 0x00);
    ASSERT_EQ(f.value.bytes.data[1], 0xFF);
    ASSERT_EQ(f.value.bytes.data[2], 0x42);
    ASSERT_EQ(f.value.bytes.data[3], 0x80);
}

/* ---- Multi-field message ---- */

TEST(test_multi_field)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_varint(&enc, 1, 100), PB_OK);
    ASSERT_EQ(pb_encode_string(&enc, 2, "test"), PB_OK);
    ASSERT_EQ(pb_encode_fixed32(&enc, 3, 0x12345678), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));

    /* Field 1: varint 100 */
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.field_num, 1);
    ASSERT_EQ(f.wire_type, PB_WIRE_VARINT);
    ASSERT_EQ(f.value.varint, 100);

    /* Field 2: string "test" */
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.field_num, 2);
    ASSERT_EQ(f.wire_type, PB_WIRE_LENGTH);
    ASSERT_EQ(f.value.bytes.len, 4);
    ASSERT_MEMEQ(f.value.bytes.data, "test");

    /* Field 3: fixed32 */
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.field_num, 3);
    ASSERT_EQ(f.wire_type, PB_WIRE_32BIT);
    ASSERT_EQ_HEX(f.value.fixed32, 0x12345678);

    ASSERT_TRUE(pb_decoder_done(&dec));
}

/* ---- Buffer overflow on encode ---- */

TEST(test_encode_overflow)
{
    uint8_t tiny[2];
    pb_encoder_t enc;
    pb_encoder_init(&enc, tiny, sizeof(tiny));
    /* varint(42) needs tag(1)+value(1)=2 bytes, OK */
    ASSERT_EQ(pb_encode_varint(&enc, 1, 42), PB_OK);
    /* Second encode must overflow */
    ASSERT_EQ(pb_encode_varint(&enc, 2, 1), PB_ERR_OVERFLOW);
}

/* ---- Truncated input on decode ---- */

TEST(test_decode_truncated)
{
    /* Just a tag byte, no value */
    uint8_t data[] = {0x08}; /* field 1, varint */
    pb_decoder_t dec;
    pb_decoder_init(&dec, data, sizeof(data));
    pb_field_t f;
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_ERR_TRUNCATED);
}

/* ---- Malformed varint (too many bytes) ---- */

TEST(test_malformed_varint)
{
    /* 11 continuation bytes — exceeds 10-byte max varint */
    uint8_t data[12];
    data[0] = 0x08; /* tag: field 1, varint */
    for (int i = 1; i < 11; i++) data[i] = 0x80;
    data[11] = 0x01;

    pb_decoder_t dec;
    pb_decoder_init(&dec, data, sizeof(data));
    pb_field_t f;
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_ERR_MALFORMED);
}

/* ---- Empty message ---- */

TEST(test_empty_message)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encoder_len(&enc), 0);

    pb_decoder_t dec;
    pb_decoder_init(&dec, g_buf, 0);
    ASSERT_TRUE(pb_decoder_done(&dec));

    pb_field_t f;
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_ERR_TRUNCATED);
}

/* ---- ZigZag roundtrip ---- */

TEST(test_zigzag_roundtrip)
{
    /* 0 -> 0 -> 0 */
    ASSERT_EQ(pb_zigzag_decode32(0), 0);
    /* -1 -> 1 -> -1 */
    ASSERT_EQ(pb_zigzag_decode32(1), -1);
    /* 1 -> 2 -> 1 */
    ASSERT_EQ(pb_zigzag_decode32(2), 1);
    /* -2 -> 3 -> -2 */
    ASSERT_EQ(pb_zigzag_decode32(3), -2);

    /* 64-bit */
    ASSERT_EQ(pb_zigzag_decode64(0), 0);
    ASSERT_EQ(pb_zigzag_decode64(1), -1);
    ASSERT_EQ(pb_zigzag_decode64(2), 1);
    ASSERT_EQ(pb_zigzag_decode64(3), -2);
}

/* ---- decoder_done correctness ---- */

TEST(test_decoder_done)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_varint(&enc, 1, 7), PB_OK);
    ASSERT_EQ(pb_encode_varint(&enc, 2, 8), PB_OK);

    pb_decoder_t dec;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));

    ASSERT_FALSE(pb_decoder_done(&dec));

    pb_field_t f;
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_FALSE(pb_decoder_done(&dec));

    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_TRUE(pb_decoder_done(&dec));
}

/* ---- Wire type correctness ---- */

TEST(test_wire_types)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_varint(&enc, 1, 1), PB_OK);
    ASSERT_EQ(pb_encode_fixed32(&enc, 2, 2), PB_OK);
    ASSERT_EQ(pb_encode_fixed64(&enc, 3, 3), PB_OK);
    ASSERT_EQ(pb_encode_string(&enc, 4, "x"), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));

    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.wire_type, PB_WIRE_VARINT);

    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.wire_type, PB_WIRE_32BIT);

    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.wire_type, PB_WIRE_64BIT);

    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.wire_type, PB_WIRE_LENGTH);
}

/* ---- Fixed64 roundtrip ---- */

TEST(test_fixed64)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    uint64_t val = 0x0102030405060708ULL;
    ASSERT_EQ(pb_encode_fixed64(&enc, 1, val), PB_OK);
    /* tag(1) + 8 bytes = 9 */
    ASSERT_EQ(pb_encoder_len(&enc), 9);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(f.wire_type, PB_WIRE_64BIT);
    ASSERT_EQ(f.value.fixed64, val);
}

/* ---- Sint64 roundtrip ---- */

TEST(test_sint64)
{
    pb_encoder_t enc;
    pb_encoder_init(&enc, g_buf, sizeof(g_buf));
    ASSERT_EQ(pb_encode_sint64(&enc, 1, -12345678LL), PB_OK);

    pb_decoder_t dec;
    pb_field_t f;
    pb_decoder_init(&dec, g_buf, pb_encoder_len(&enc));
    ASSERT_EQ(pb_decode_field(&dec, &f), PB_OK);
    ASSERT_EQ(pb_zigzag_decode64(f.value.varint), -12345678LL);
}

/* ---- Null param checks ---- */

TEST(test_null_params)
{
    ASSERT_EQ(pb_encode_varint(NULL, 1, 1), PB_ERR_PARAMS);
    ASSERT_EQ(pb_encode_fixed32(NULL, 1, 1), PB_ERR_PARAMS);
    ASSERT_EQ(pb_encode_fixed64(NULL, 1, 1), PB_ERR_PARAMS);
    ASSERT_EQ(pb_encode_string(NULL, 1, "x"), PB_ERR_PARAMS);
    ASSERT_EQ(pb_encode_bytes(NULL, 1, NULL, 0), PB_ERR_PARAMS);
    ASSERT_EQ(pb_decode_field(NULL, NULL), PB_ERR_PARAMS);
    ASSERT_EQ(pb_encoder_len(NULL), 0);
    ASSERT_TRUE(pb_decoder_done(NULL));
}

TEST_MAIN("protobuf_lite")
    RUN(test_varint_small);
    RUN(test_varint_medium);
    RUN(test_varint_large);
    RUN(test_sint32_positive);
    RUN(test_sint32_negative);
    RUN(test_sint32_minus_one);
    RUN(test_sint32_min);
    RUN(test_fixed32);
    RUN(test_float_zero);
    RUN(test_float_positive);
    RUN(test_float_negative);
    RUN(test_string);
    RUN(test_bytes);
    RUN(test_multi_field);
    RUN(test_encode_overflow);
    RUN(test_decode_truncated);
    RUN(test_malformed_varint);
    RUN(test_empty_message);
    RUN(test_zigzag_roundtrip);
    RUN(test_decoder_done);
    RUN(test_wire_types);
    RUN(test_fixed64);
    RUN(test_sint64);
    RUN(test_null_params);
TEST_MAIN_END()
