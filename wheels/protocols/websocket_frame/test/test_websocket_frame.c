#include "test_harness.h"
#include "websocket_frame.h"
#include <string.h>

TEST(test_parse_unmasked_text)
{
    uint8_t data[] = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};
    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(data, sizeof(data), &f), WS_OK);
    ASSERT_TRUE(f.fin);
    ASSERT_EQ(f.opcode, WS_OPCODE_TEXT);
    ASSERT_FALSE(f.masked);
    ASSERT_EQ(f.payload_len, 5);
    ASSERT_EQ(memcmp(f.payload, "Hello", 5), 0);
    ASSERT_EQ(f.frame_len, 7);
}

TEST(test_parse_masked_text)
{
    uint8_t data[] = {0x81, 0x85,
                      0x37, 0xfa, 0x21, 0x3d,
                      0x7f, 0x9f, 0x4d, 0x51, 0x58};
    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(data, sizeof(data), &f), WS_OK);
    ASSERT_TRUE(f.masked);
    ASSERT_EQ(f.payload_len, 5);
    ASSERT_EQ(f.frame_len, 11);
    uint8_t unmasked[5];
    memcpy(unmasked, f.payload, 5);
    ws_mask_payload(unmasked, 5, f.mask_key);
    ASSERT_EQ(memcmp(unmasked, "Hello", 5), 0);
}

TEST(test_parse_ping)
{
    uint8_t data[] = {0x89, 0x00};
    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(data, sizeof(data), &f), WS_OK);
    ASSERT_TRUE(f.fin);
    ASSERT_EQ(f.opcode, WS_OPCODE_PING);
    ASSERT_EQ(f.payload_len, 0);
}

TEST(test_parse_close)
{
    uint8_t data[] = {0x88, 0x02, 0x03, 0xE8};
    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(data, sizeof(data), &f), WS_OK);
    ASSERT_EQ(f.opcode, WS_OPCODE_CLOSE);
    ASSERT_EQ(f.payload_len, 2);
    uint16_t code = ((uint16_t)f.payload[0] << 8) | f.payload[1];
    ASSERT_EQ(code, 1000);
}

TEST(test_parse_16bit_length)
{
    uint8_t data[4 + 200];
    data[0] = 0x82;
    data[1] = 126;
    data[2] = 0;
    data[3] = 200;
    memset(&data[4], 0xAA, 200);
    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(data, sizeof(data), &f), WS_OK);
    ASSERT_EQ(f.payload_len, 200);
    ASSERT_EQ(f.frame_len, 204);
}

TEST(test_parse_incomplete)
{
    uint8_t data[] = {0x81};
    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(data, 1, &f), WS_ERR_INCOMPLETE);
}

TEST(test_build_unmasked)
{
    uint8_t buf[64];
    size_t out_len;
    ASSERT_EQ(ws_frame_build(buf, sizeof(buf), &out_len,
                             true, WS_OPCODE_TEXT, NULL,
                             (const uint8_t *)"Hello", 5), WS_OK);
    ASSERT_EQ(out_len, 7);
    ASSERT_EQ(buf[0], 0x81);
    ASSERT_EQ(buf[1], 0x05);
    ASSERT_EQ(memcmp(&buf[2], "Hello", 5), 0);
}

TEST(test_build_masked)
{
    uint8_t buf[64];
    size_t out_len;
    uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
    ASSERT_EQ(ws_frame_build(buf, sizeof(buf), &out_len,
                             true, WS_OPCODE_TEXT, mask,
                             (const uint8_t *)"Hello", 5), WS_OK);
    ASSERT_EQ(out_len, 11);
    ASSERT_EQ(buf[1] & 0x80, 0x80);

    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(buf, out_len, &f), WS_OK);
    uint8_t unmasked[5];
    memcpy(unmasked, f.payload, 5);
    ws_mask_payload(unmasked, 5, f.mask_key);
    ASSERT_EQ(memcmp(unmasked, "Hello", 5), 0);
}

TEST(test_build_overflow)
{
    uint8_t buf[4];
    size_t out_len;
    ASSERT_EQ(ws_frame_build(buf, sizeof(buf), &out_len,
                             true, WS_OPCODE_TEXT, NULL,
                             (const uint8_t *)"Hello", 5), WS_ERR_OVERFLOW);
}

TEST(test_roundtrip)
{
    uint8_t buf[128];
    size_t out_len;
    const uint8_t payload[] = "Test roundtrip data";
    ASSERT_EQ(ws_frame_build(buf, sizeof(buf), &out_len,
                             true, WS_OPCODE_BINARY, NULL,
                             payload, sizeof(payload) - 1), WS_OK);
    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(buf, out_len, &f), WS_OK);
    ASSERT_TRUE(f.fin);
    ASSERT_EQ(f.opcode, WS_OPCODE_BINARY);
    ASSERT_EQ(f.payload_len, sizeof(payload) - 1);
    ASSERT_EQ(memcmp(f.payload, payload, f.payload_len), 0);
}

TEST(test_null_safety)
{
    ws_frame_t f;
    ASSERT_EQ(ws_frame_parse(NULL, 0, &f), WS_ERR_PARAM);
    size_t out;
    ASSERT_EQ(ws_frame_build(NULL, 0, &out, true, WS_OPCODE_TEXT,
                             NULL, NULL, 0), WS_ERR_PARAM);
}

TEST_MAIN("websocket_frame")
    RUN(test_parse_unmasked_text);
    RUN(test_parse_masked_text);
    RUN(test_parse_ping);
    RUN(test_parse_close);
    RUN(test_parse_16bit_length);
    RUN(test_parse_incomplete);
    RUN(test_build_unmasked);
    RUN(test_build_masked);
    RUN(test_build_overflow);
    RUN(test_roundtrip);
    RUN(test_null_safety);
TEST_MAIN_END()
