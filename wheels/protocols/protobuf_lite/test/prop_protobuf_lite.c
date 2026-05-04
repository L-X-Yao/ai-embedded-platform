/*
 * prop_protobuf_lite.c — property-based tests for protobuf_lite
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_protobuf_lite.c ../protobuf_lite.c -o prop_protobuf_lite
 */

#include "test_harness.h"
#include "property_test.h"
#include "../protobuf_lite.h"
#include <string.h>

/* Property: encode varint then decode roundtrip */
PROPERTY(varint_roundtrip, 20000) {
    uint8_t field_num = (uint8_t)ARBI_RANGE(seed, 1, 15);
    uint64_t value = ((uint64_t)ARBI_UINT32(seed) << 16) | ARBI_UINT16(seed);

    uint8_t buf[64];
    pb_encoder_t enc;
    pb_encoder_init(&enc, buf, sizeof(buf));
    pb_err_t err = pb_encode_varint(&enc, field_num, value);
    PROP_ASSERT_EQ(err, PB_OK);

    pb_decoder_t dec;
    pb_decoder_init(&dec, buf, pb_encoder_len(&enc));
    pb_field_t field;
    memset(&field, 0, sizeof(field));
    err = pb_decode_field(&dec, &field);
    PROP_ASSERT_EQ(err, PB_OK);
    PROP_ASSERT_EQ(field.field_num, field_num);
    PROP_ASSERT_EQ(field.wire_type, PB_WIRE_VARINT);
    PROP_ASSERT_EQ(field.value.varint, value);
}

/* Property: encode bytes then decode roundtrip */
PROPERTY(bytes_roundtrip, 10000) {
    uint8_t field_num = (uint8_t)ARBI_RANGE(seed, 1, 15);
    uint8_t data[64];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 64);
    ARBI_FILL(seed, data, len);

    uint8_t buf[128];
    pb_encoder_t enc;
    pb_encoder_init(&enc, buf, sizeof(buf));
    pb_err_t err = pb_encode_bytes(&enc, field_num, data, len);
    PROP_ASSERT_EQ(err, PB_OK);

    pb_decoder_t dec;
    pb_decoder_init(&dec, buf, pb_encoder_len(&enc));
    pb_field_t field;
    memset(&field, 0, sizeof(field));
    err = pb_decode_field(&dec, &field);
    PROP_ASSERT_EQ(err, PB_OK);
    PROP_ASSERT_EQ(field.field_num, field_num);
    PROP_ASSERT_EQ(field.wire_type, PB_WIRE_LENGTH);
    PROP_ASSERT_EQ(field.value.bytes.len, len);
    if (len > 0) {
        PROP_ASSERT_MEM_EQ(field.value.bytes.data, data, len);
    }
}

/* Property: zigzag encode/decode roundtrip */
PROPERTY(zigzag_roundtrip, 10000) {
    int32_t val32 = (int32_t)ARBI_UINT32(seed);
    /* ZigZag encode then decode must be identity */
    uint32_t zz = (uint32_t)((val32 << 1) ^ (val32 >> 31));
    int32_t decoded = pb_zigzag_decode32(zz);
    PROP_ASSERT_EQ(decoded, val32);
}

/* Property: decode of arbitrary bytes never crashes */
PROPERTY(decode_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[128];
    size_t len = ARBI_SIZE(seed, 128);
    ARBI_FILL(seed, garbage, len);

    pb_decoder_t dec;
    pb_decoder_init(&dec, garbage, (uint16_t)len);
    pb_field_t field;
    /* Consume all fields until error or done */
    for (int i = 0; i < 20; i++) {
        memset(&field, 0, sizeof(field));
        pb_err_t err = pb_decode_field(&dec, &field);
        if (err != PB_OK)
            break;
    }
}

TEST_MAIN("protobuf_lite properties") {
    RUN(varint_roundtrip);
    RUN(bytes_roundtrip);
    RUN(zigzag_roundtrip);
    RUN(decode_no_crash);
}
TEST_MAIN_END()
