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
    ASSERT_EQ(coap_parse(data, sizeof(data), &pkt), COAP_OK);
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
    ASSERT_EQ(coap_parse(data, sizeof(data), &pkt), COAP_OK);
    ASSERT_EQ(pkt.code, COAP_CODE_CONTENT);
    ASSERT_EQ(pkt.payload_len, 2);
    ASSERT_EQ(memcmp(pkt.payload, "Hi", 2), 0);
}

TEST(test_parse_ack_empty)
{
    uint8_t data[] = {0x60, 0x00, 0x12, 0x34};
    coap_packet_t pkt;
    ASSERT_EQ(coap_parse(data, sizeof(data), &pkt), COAP_OK);
    ASSERT_EQ(pkt.type, COAP_TYPE_ACK);
    ASSERT_EQ(pkt.code, COAP_CODE_EMPTY);
    ASSERT_EQ(pkt.option_count, 0);
    ASSERT_EQ(pkt.payload_len, 0);
}

TEST(test_parse_bad_version)
{
    uint8_t data[] = {0x00, 0x01, 0x00, 0x01};
    coap_packet_t pkt;
    ASSERT_EQ(coap_parse(data, sizeof(data), &pkt), COAP_ERR_MALFORMED);
}

TEST(test_parse_incomplete)
{
    uint8_t data[] = {0x40, 0x01};
    coap_packet_t pkt;
    ASSERT_EQ(coap_parse(data, 2, &pkt), COAP_ERR_INCOMPLETE);
}

TEST(test_build_simple_get)
{
    uint8_t buf[64];
    size_t out_len;
    uint8_t token[] = {0xAB};
    coap_option_t opts[] = {
        {.number = COAP_OPT_URI_PATH,
         .value = (const uint8_t *)"temp",
         .value_len = 4}
    };
    ASSERT_EQ(coap_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         token, 1, opts, 1, NULL, 0), COAP_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_parse(buf, out_len, &pkt), COAP_OK);
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
    ASSERT_EQ(coap_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_NON, COAP_CODE_CONTENT, 0x0002,
                         NULL, 0, NULL, 0,
                         (const uint8_t *)"25.5", 4), COAP_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_parse(buf, out_len, &pkt), COAP_OK);
    ASSERT_EQ(pkt.type, COAP_TYPE_NON);
    ASSERT_EQ(pkt.payload_len, 4);
    ASSERT_EQ(memcmp(pkt.payload, "25.5", 4), 0);
}

TEST(test_build_overflow)
{
    uint8_t buf[4];
    size_t out_len;
    uint8_t token[] = {0x01, 0x02, 0x03, 0x04};
    ASSERT_EQ(coap_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0001,
                         token, 4, NULL, 0, NULL, 0), COAP_ERR_OVERFLOW);
}

TEST(test_roundtrip_multi_options)
{
    uint8_t buf[128];
    size_t out_len;
    coap_option_t opts[] = {
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
    ASSERT_EQ(coap_build(buf, sizeof(buf), &out_len,
                         COAP_TYPE_CON, COAP_CODE_GET, 0x0003,
                         NULL, 0, opts, 3,
                         (const uint8_t *)"data", 4), COAP_OK);

    coap_packet_t pkt;
    ASSERT_EQ(coap_parse(buf, out_len, &pkt), COAP_OK);
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
    ASSERT_EQ(coap_parse(NULL, 0, &pkt), COAP_ERR_PARAM);
    size_t out;
    ASSERT_EQ(coap_build(NULL, 0, &out, COAP_TYPE_CON, 0, 0,
                         NULL, 0, NULL, 0, NULL, 0), COAP_ERR_PARAM);
}

TEST(test_parse_bare_payload_marker)
{
    uint8_t data[] = {0x40, 0x01, 0x00, 0x01, 0xFF};
    coap_packet_t pkt;
    ASSERT_EQ(coap_parse(data, sizeof(data), &pkt), COAP_ERR_MALFORMED);
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
TEST_MAIN_END()
