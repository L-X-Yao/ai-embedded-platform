// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "coap_packet.h"
#include <string.h>

TEST(test_parse_con_get)
{
    /* CON GET /test with token 0xABCD, MID 0x1234 */
    uint8_t data[] = {
        0x42,       /* ver=1, type=CON, tkl=2 */
        0x01,       /* code=0.01 GET */
        0x12, 0x34, /* message_id */
        0xAB, 0xCD, /* token */
        0xB4,       /* opt delta=11 (Uri-Path), len=4 */
        't', 'e', 's', 't'
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.version, 1);
    ASSERT_EQ(pkt.type, COAP_TYPE_CON);
    ASSERT_EQ(pkt.token_len, 2);
    ASSERT_EQ(pkt.code, COAP_CODE_GET);
    ASSERT_EQ(pkt.message_id, 0x1234);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, COAP_OPT_URI_PATH);
    ASSERT_EQ(pkt.options[0].value_len, 4);
    ASSERT_EQ(memcmp(pkt.options[0].value, "test", 4), 0);
    ASSERT_EQ(pkt.payload_len, 0);
}

TEST(test_parse_with_payload)
{
    uint8_t data[] = {
        0x40,       /* CON, tkl=0 */
        0x45,       /* 2.05 Content */
        0x00, 0x01,
        0xFF,       /* payload marker */
        'H', 'i'
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.code, COAP_CODE_CONTENT);
    ASSERT_EQ(pkt.payload_len, 2);
    ASSERT_EQ(memcmp(pkt.payload, "Hi", 2), 0);
}

TEST(test_parse_ack_empty)
{
    uint8_t data[] = {0x60, 0x00, 0x12, 0x34};
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.type, COAP_TYPE_ACK);
    ASSERT_EQ(pkt.code, COAP_CODE_EMPTY);
    ASSERT_EQ(pkt.option_count, 0);
    ASSERT_EQ(pkt.payload_len, 0);
}

TEST(test_parse_bad_version)
{
    uint8_t data[] = {0x00, 0x01, 0x00, 0x01};
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_MALFORMED);
}

TEST(test_parse_incomplete)
{
    uint8_t data[] = {0x40, 0x01};
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, 2, &pkt), COAP_PACKET_ERR_INCOMPLETE);
}

TEST(test_build_simple_get)
{
    uint8_t buf[64];
    size_t out_len;
    uint8_t token[] = {0xAB};
    coap_packet_option_t opts[] = {
        {.number = COAP_OPT_URI_PATH,
         .value = (const uint8_t *)"temp",
         .value_len = 4}
    };
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         token, 1, opts, 1, NULL, 0), COAP_PACKET_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(buf, out_len, &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.type, COAP_TYPE_CON);
    ASSERT_EQ(pkt.code, COAP_CODE_GET);
    ASSERT_EQ(pkt.token_len, 1);
    ASSERT_EQ(pkt.token[0], 0xAB);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, COAP_OPT_URI_PATH);
}

TEST(test_build_with_payload)
{
    uint8_t buf[64];
    size_t out_len;
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_NON, COAP_CODE_CONTENT, 0x0002,
                         NULL, 0, NULL, 0,
                         (const uint8_t *)"25.5", 4), COAP_PACKET_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(buf, out_len, &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.type, COAP_TYPE_NON);
    ASSERT_EQ(pkt.payload_len, 4);
    ASSERT_EQ(memcmp(pkt.payload, "25.5", 4), 0);
}

TEST(test_build_overflow)
{
    uint8_t buf[4];
    size_t out_len;
    uint8_t token[] = {0x01, 0x02, 0x03, 0x04};
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         token, 4, NULL, 0, NULL, 0), COAP_PACKET_ERR_OVERFLOW);
}

TEST(test_roundtrip_multi_options)
{
    uint8_t buf[128];
    size_t out_len;
    coap_packet_option_t opts[] = {
        {.number = COAP_OPT_URI_PATH,
         .value = (const uint8_t *)"api",
         .value_len = 3},
        {.number = COAP_OPT_URI_PATH,
         .value = (const uint8_t *)"sensor",
         .value_len = 6},
        {.number = COAP_OPT_URI_QUERY,
         .value = (const uint8_t *)"id=1",
         .value_len = 4},
    };
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0003,
                         NULL, 0, opts, 3,
                         (const uint8_t *)"data", 4), COAP_PACKET_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(buf, out_len, &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 3);
    ASSERT_EQ(pkt.options[0].number, COAP_OPT_URI_PATH);
    ASSERT_EQ(pkt.options[0].value_len, 3);
    ASSERT_EQ(pkt.options[1].number, COAP_OPT_URI_PATH);
    ASSERT_EQ(pkt.options[1].value_len, 6);
    ASSERT_EQ(pkt.options[2].number, COAP_OPT_URI_QUERY);
    ASSERT_EQ(pkt.payload_len, 4);
}

TEST(test_null_safety)
{
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(NULL, 0, &pkt), COAP_PACKET_ERR_PARAM);
    size_t out;
    ASSERT_EQ(coap_packet_build(NULL, 0, &out, COAP_TYPE_CON, 0, 0,
                         NULL, 0, NULL, 0, NULL, 0), COAP_PACKET_ERR_PARAM);
}

TEST(test_parse_bare_payload_marker)
{
    uint8_t data[] = {0x40, 0x01, 0x00, 0x01, 0xFF};
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_MALFORMED);
}

TEST(test_parse_too_many_options)
{
    uint8_t data[4 + 9];
    data[0] = 0x40;       /* ver=1, CON, tkl=0 */
    data[1] = 0x01;       /* GET */
    data[2] = 0x00;
    data[3] = 0x01;       /* MID */
    for (int i = 0; i < 9; i++)
        data[4 + i] = 0x10; /* delta=1, len=0 */
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_TOO_MANY);
    ASSERT_EQ(pkt.option_count, COAP_MAX_OPTIONS);
}

/* ---- parse: token_len > 8 is malformed ---- */

TEST(test_parse_token_too_long)
{
    /* tkl = 9 is reserved/malformed per RFC 7252 */
    uint8_t data[] = {0x49, 0x01, 0x00, 0x01};
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_MALFORMED);
}

/* ---- parse: token truncated (token_len > remaining bytes) ---- */

TEST(test_parse_token_truncated)
{
    /* tkl=4 but only 2 token bytes provided */
    uint8_t data[] = {0x44, 0x01, 0x00, 0x01, 0xAA, 0xBB};
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_INCOMPLETE);
}

/* ---- parse: delta == 13 extended (one extra byte) ---- */

TEST(test_parse_opt_delta13_extended)
{
    /*
     * Header: ver=1 CON tkl=0
     * Option byte: 0xD0 = delta_nibble=13 (extended), len_nibble=0
     * Extended delta byte: 0x00 → actual delta = 0 + 13 = 13 (Uri-Host)
     * Option value: "ex" (2 bytes)
     * But we set len_nibble=2 in the option byte: 0xD2
     */
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01, /* header: CON GET MID=1 */
        0xD2,                   /* delta_nibble=13 (ext), len_nibble=2 */
        0x00,                   /* extended delta byte: 0 → delta=13 */
        'e', 'x'                /* option value */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, 13); /* COAP_OPT_URI_HOST */
    ASSERT_EQ(pkt.options[0].value_len, 2);
}

/* ---- parse: delta == 13, extended byte missing (truncated) ---- */

TEST(test_parse_opt_delta13_truncated)
{
    /* Option byte has delta_nibble=13 but no extended byte follows */
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01, /* header */
        0xD0                    /* delta_nibble=13, no extended byte */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_INCOMPLETE);
}

/* ---- parse: delta == 14 extended (two extra bytes) ---- */

TEST(test_parse_opt_delta14_extended)
{
    /*
     * delta_nibble=14 (0xE), len_nibble=0 → two extra delta bytes follow
     * Extended delta = (byte[0]<<8 | byte[1]) + 269
     * To get delta=269: bytes must be 0x00,0x00
     * Option number = 269 (proxy-uri option number in CoAP)
     */
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01, /* header: CON GET MID=1 */
        0xE0,                   /* delta_nibble=14 (2-byte ext), len_nibble=0 */
        0x00, 0x00              /* extended delta: (0<<8|0)+269 = 269 */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, 269);
    ASSERT_EQ(pkt.options[0].value_len, 0);
}

/* ---- parse: delta == 14 with only one byte available (truncated) ---- */

TEST(test_parse_opt_delta14_truncated)
{
    /* delta_nibble=14 needs 2 extra bytes, only 1 provided */
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01,
        0xE0, 0x00              /* only one extended byte */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_INCOMPLETE);
}

/* ---- parse: delta == 15 reserved, must be rejected ---- */

TEST(test_parse_opt_delta15_reserved)
{
    /* delta_nibble=15 is reserved → MALFORMED */
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01,
        0xF0                    /* delta_nibble=15 (reserved) */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_MALFORMED);
}

/* ---- parse: opt_len == 13 extended ---- */

TEST(test_parse_opt_len13_extended)
{
    /*
     * delta_nibble=0, len_nibble=13 → one extra length byte follows
     * Extra byte=0 → actual len = 0+13 = 13
     * We need 13 bytes of value data
     */
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01, /* header */
        0x0D,                   /* delta_nibble=0, len_nibble=13 (ext) */
        0x00,                   /* extended len byte: 0 → len=13 */
        'A','B','C','D','E','F','G','H','I','J','K','L','M' /* 13 bytes */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, 0);
    ASSERT_EQ(pkt.options[0].value_len, 13);
}

/* ---- parse: opt_len == 13, extended byte missing ---- */

TEST(test_parse_opt_len13_truncated)
{
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01,
        0x0D                    /* len_nibble=13 but no extended byte */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_INCOMPLETE);
}

/* ---- parse: opt_len == 14 extended (two bytes) ---- */

TEST(test_parse_opt_len14_extended)
{
    /*
     * delta_nibble=0, len_nibble=14 → two extra length bytes
     * Extra bytes: 0x00,0x00 → actual len = (0<<8|0)+269 = 269
     * We need 269 bytes of value data — use static buffer
     */
    static uint8_t data[4 + 1 + 2 + 269];
    data[0] = 0x40; data[1] = 0x01; data[2] = 0x00; data[3] = 0x01;
    data[4] = 0x0E;  /* delta_nibble=0, len_nibble=14 (2-byte ext) */
    data[5] = 0x00;  /* extended len high byte */
    data[6] = 0x00;  /* extended len low byte → len=(0<<8|0)+269=269 */
    /* 269 bytes of value at data[7..275] — already zero-initialized (static) */

    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].value_len, 269);
}

/* ---- parse: opt_len == 14, only one byte available ---- */

TEST(test_parse_opt_len14_truncated)
{
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01,
        0x0E, 0x00              /* len_nibble=14, only one extended byte */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_INCOMPLETE);
}

/* ---- parse: opt_len == 15 reserved ---- */

TEST(test_parse_opt_len15_reserved)
{
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01,
        0x0F                    /* delta_nibble=0, len_nibble=15 (reserved) */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_MALFORMED);
}

/* ---- parse: option value extends beyond packet end ---- */

TEST(test_parse_opt_value_truncated)
{
    /* delta=1, len=10 but only 4 bytes remain in packet */
    uint8_t data[] = {
        0x40, 0x01, 0x00, 0x01,
        0x1A,                   /* delta_nibble=1, len_nibble=10 */
        'A', 'B', 'C', 'D'     /* only 4 bytes — need 10 */
    };
    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(data, sizeof(data), &pkt), COAP_PACKET_ERR_INCOMPLETE);
}

/* ---- build: token_len > 8 rejected ---- */

TEST(test_build_token_too_long)
{
    uint8_t buf[64];
    size_t out_len;
    uint8_t token[9] = {0};
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         token, 9, NULL, 0, NULL, 0),
              COAP_PACKET_ERR_PARAM);
}

/* ---- build: option_count > 0 but options pointer is NULL ---- */

TEST(test_build_options_null_with_count)
{
    uint8_t buf[64];
    size_t out_len;
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         NULL, 0, NULL, 1, NULL, 0),
              COAP_PACKET_ERR_PARAM);
}

/* ---- build: out_len NULL ---- */

TEST(test_build_out_len_null)
{
    uint8_t buf[64];
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), NULL,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         NULL, 0, NULL, 0, NULL, 0),
              COAP_PACKET_ERR_PARAM);
}

/* ---- build: option value overflows buffer ---- */

TEST(test_build_option_overflow)
{
    /* Buffer is large enough for header but not for a 50-byte option value */
    uint8_t buf[10];
    size_t out_len;
    uint8_t val[50];
    int i;
    for (i = 0; i < 50; i++) val[i] = (uint8_t)i;
    coap_packet_option_t opts[] = {
        {.number = COAP_OPT_URI_PATH, .value = val, .value_len = 50}
    };
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         NULL, 0, opts, 1, NULL, 0),
              COAP_PACKET_ERR_OVERFLOW);
}

/* ---- build: payload overflows buffer ---- */

TEST(test_build_payload_overflow)
{
    /* Buffer fits header (4 bytes) + marker (1 byte) but not a 50-byte payload */
    uint8_t buf[10];
    size_t out_len;
    uint8_t payload[50];
    int i;
    for (i = 0; i < 50; i++) payload[i] = (uint8_t)i;
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         NULL, 0, NULL, 0, payload, 50),
              COAP_PACKET_ERR_OVERFLOW);
}

/* ---- build + parse roundtrip with delta >= 13 option numbers ---- */

TEST(test_build_opt_delta13_roundtrip)
{
    /*
     * Use option number 13 (Uri-Host) to trigger the extended-delta-13 path
     * in encode_opt_header when prev_num=0 → delta=13.
     */
    uint8_t buf[64];
    size_t out_len;
    const uint8_t host_val[] = "192.168.1.1";
    coap_packet_option_t opts[] = {
        {.number = COAP_OPT_URI_HOST,
         .value  = host_val,
         .value_len = 11}
    };
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0010,
                         NULL, 0, opts, 1, NULL, 0), COAP_PACKET_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(buf, out_len, &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, COAP_OPT_URI_HOST);
    ASSERT_EQ(pkt.options[0].value_len, 11);
}

/* ---- build + parse roundtrip with delta >= 269 option numbers ---- */

TEST(test_build_opt_delta14_roundtrip)
{
    /*
     * Use option number 269 to trigger the extended-delta-14 (two-byte) path.
     * prev_num=0 → delta=269, which requires the two-byte extended format.
     */
    uint8_t buf[32];
    size_t out_len;
    const uint8_t val[] = {0x01};
    coap_packet_option_t opts[] = {
        {.number = 269, .value = val, .value_len = 1}
    };
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0020,
                         NULL, 0, opts, 1, NULL, 0), COAP_PACKET_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(buf, out_len, &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, 269);
    ASSERT_EQ(pkt.options[0].value_len, 1);
}

/* ---- build + parse roundtrip with opt_len >= 13 ---- */

TEST(test_build_opt_len13_roundtrip)
{
    /*
     * Option value of 13 bytes triggers the extended-length-13 encoding
     * in encode_opt_header for the length field.
     */
    uint8_t buf[64];
    size_t out_len;
    const uint8_t val[13] = {
        'A','B','C','D','E','F','G','H','I','J','K','L','M'
    };
    coap_packet_option_t opts[] = {
        {.number = COAP_OPT_URI_PATH, .value = val, .value_len = 13}
    };
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0030,
                         NULL, 0, opts, 1, NULL, 0), COAP_PACKET_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(buf, out_len, &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, COAP_OPT_URI_PATH);
    ASSERT_EQ(pkt.options[0].value_len, 13);
}

/* ---- build + parse roundtrip with opt_len >= 269 ---- */

TEST(test_build_opt_len14_roundtrip)
{
    /*
     * Option value of 269 bytes triggers the extended-length-14 (two-byte)
     * encoding in encode_opt_header for the length field.
     */
    static uint8_t buf[512];
    static uint8_t val[269];
    size_t i;
    size_t out_len;
    for (i = 0; i < 269; i++) val[i] = (uint8_t)(i & 0xFF);

    coap_packet_option_t opts[] = {
        {.number = COAP_OPT_URI_PATH, .value = val, .value_len = 269}
    };
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0040,
                         NULL, 0, opts, 1, NULL, 0), COAP_PACKET_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_packet_parse(buf, out_len, &pkt), COAP_PACKET_OK);
    ASSERT_EQ(pkt.option_count, 1);
    ASSERT_EQ(pkt.options[0].number, COAP_OPT_URI_PATH);
    ASSERT_EQ(pkt.options[0].value_len, 269);
}

/* ---- build: out-of-order option numbers rejected ---- */

TEST(test_build_options_out_of_order)
{
    uint8_t buf[64];
    size_t out_len;
    coap_packet_option_t opts[] = {
        {.number = COAP_OPT_URI_QUERY, .value = NULL, .value_len = 0},
        {.number = COAP_OPT_URI_PATH,  .value = NULL, .value_len = 0},
    };
    /* URI_QUERY (15) before URI_PATH (11) is out of order */
    ASSERT_EQ(coap_packet_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         NULL, 0, opts, 2, NULL, 0),
              COAP_PACKET_ERR_MALFORMED);
}

TEST_MAIN("coap_packet")
    RUN(test_parse_con_get);
    RUN(test_parse_with_payload);
    RUN(test_parse_ack_empty);
    RUN(test_parse_bad_version);
    RUN(test_parse_incomplete);
    RUN(test_build_simple_get);
    RUN(test_build_with_payload);
    RUN(test_build_overflow);
    RUN(test_roundtrip_multi_options);
    RUN(test_null_safety);
    RUN(test_parse_bare_payload_marker);
    RUN(test_parse_too_many_options);
    RUN(test_parse_token_too_long);
    RUN(test_parse_token_truncated);
    RUN(test_parse_opt_delta13_extended);
    RUN(test_parse_opt_delta13_truncated);
    RUN(test_parse_opt_delta14_extended);
    RUN(test_parse_opt_delta14_truncated);
    RUN(test_parse_opt_delta15_reserved);
    RUN(test_parse_opt_len13_extended);
    RUN(test_parse_opt_len13_truncated);
    RUN(test_parse_opt_len14_extended);
    RUN(test_parse_opt_len14_truncated);
    RUN(test_parse_opt_len15_reserved);
    RUN(test_parse_opt_value_truncated);
    RUN(test_build_token_too_long);
    RUN(test_build_options_null_with_count);
    RUN(test_build_out_len_null);
    RUN(test_build_option_overflow);
    RUN(test_build_payload_overflow);
    RUN(test_build_opt_delta13_roundtrip);
    RUN(test_build_opt_delta14_roundtrip);
    RUN(test_build_opt_len13_roundtrip);
    RUN(test_build_opt_len14_roundtrip);
    RUN(test_build_options_out_of_order);
TEST_MAIN_END()
