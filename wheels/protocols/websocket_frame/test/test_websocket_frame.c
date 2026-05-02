// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "websocket_frame.h"
#include <string.h>

TEST(test_parse_unmasked_text)
{
    uint8_t data[] = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, sizeof(data), &f), WEBSOCKET_FRAME_OK);
    ASSERT_TRUE(f.fin);
    ASSERT_EQ(f.opcode, WEBSOCKET_FRAME_OPCODE_TEXT);
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
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, sizeof(data), &f), WEBSOCKET_FRAME_OK);
    ASSERT_TRUE(f.masked);
    ASSERT_EQ(f.payload_len, 5);
    ASSERT_EQ(f.frame_len, 11);
    uint8_t unmasked[5];
    memcpy(unmasked, f.payload, 5);
    websocket_frame_mask_payload(unmasked, 5, f.mask_key);
    ASSERT_EQ(memcmp(unmasked, "Hello", 5), 0);
}

TEST(test_parse_ping)
{
    uint8_t data[] = {0x89, 0x00};
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, sizeof(data), &f), WEBSOCKET_FRAME_OK);
    ASSERT_TRUE(f.fin);
    ASSERT_EQ(f.opcode, WEBSOCKET_FRAME_OPCODE_PING);
    ASSERT_EQ(f.payload_len, 0);
}

TEST(test_parse_close)
{
    uint8_t data[] = {0x88, 0x02, 0x03, 0xE8};
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, sizeof(data), &f), WEBSOCKET_FRAME_OK);
    ASSERT_EQ(f.opcode, WEBSOCKET_FRAME_OPCODE_CLOSE);
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
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, sizeof(data), &f), WEBSOCKET_FRAME_OK);
    ASSERT_EQ(f.payload_len, 200);
    ASSERT_EQ(f.frame_len, 204);
}

TEST(test_parse_incomplete)
{
    uint8_t data[] = {0x81};
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, 1, &f), WEBSOCKET_FRAME_ERR_INCOMPLETE);
}

TEST(test_build_unmasked)
{
    uint8_t buf[64];
    size_t out_len;
    ASSERT_EQ(websocket_frame_build(buf, sizeof(buf), &out_len,
                             true, WEBSOCKET_FRAME_OPCODE_TEXT, NULL,
                             (const uint8_t *)"Hello", 5), WEBSOCKET_FRAME_OK);
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
    ASSERT_EQ(websocket_frame_build(buf, sizeof(buf), &out_len,
                             true, WEBSOCKET_FRAME_OPCODE_TEXT, mask,
                             (const uint8_t *)"Hello", 5), WEBSOCKET_FRAME_OK);
    ASSERT_EQ(out_len, 11);
    ASSERT_EQ(buf[1] & 0x80, 0x80);

    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(buf, out_len, &f), WEBSOCKET_FRAME_OK);
    uint8_t unmasked[5];
    memcpy(unmasked, f.payload, 5);
    websocket_frame_mask_payload(unmasked, 5, f.mask_key);
    ASSERT_EQ(memcmp(unmasked, "Hello", 5), 0);
}

TEST(test_build_overflow)
{
    uint8_t buf[4];
    size_t out_len;
    ASSERT_EQ(websocket_frame_build(buf, sizeof(buf), &out_len,
                             true, WEBSOCKET_FRAME_OPCODE_TEXT, NULL,
                             (const uint8_t *)"Hello", 5), WEBSOCKET_FRAME_ERR_OVERFLOW);
}

TEST(test_roundtrip)
{
    uint8_t buf[128];
    size_t out_len;
    const uint8_t payload[] = "Test roundtrip data";
    ASSERT_EQ(websocket_frame_build(buf, sizeof(buf), &out_len,
                             true, WEBSOCKET_FRAME_OPCODE_BINARY, NULL,
                             payload, sizeof(payload) - 1), WEBSOCKET_FRAME_OK);
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(buf, out_len, &f), WEBSOCKET_FRAME_OK);
    ASSERT_TRUE(f.fin);
    ASSERT_EQ(f.opcode, WEBSOCKET_FRAME_OPCODE_BINARY);
    ASSERT_EQ(f.payload_len, sizeof(payload) - 1);
    ASSERT_EQ(memcmp(f.payload, payload, f.payload_len), 0);
}

TEST(test_null_safety)
{
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(NULL, 0, &f), WEBSOCKET_FRAME_ERR_PARAM);
    size_t out;
    ASSERT_EQ(websocket_frame_build(NULL, 0, &out, true, WEBSOCKET_FRAME_OPCODE_TEXT,
                             NULL, NULL, 0), WEBSOCKET_FRAME_ERR_PARAM);
}

TEST(test_parse_reserved_opcode)
{
    websocket_frame_t f;
    uint8_t d3[] = {0x83, 0x00};
    ASSERT_EQ(websocket_frame_parse(d3, sizeof(d3), &f), WEBSOCKET_FRAME_ERR_PROTOCOL);
    uint8_t db[] = {0x8B, 0x00};
    ASSERT_EQ(websocket_frame_parse(db, sizeof(db), &f), WEBSOCKET_FRAME_ERR_PROTOCOL);
    uint8_t df[] = {0x8F, 0x00};
    ASSERT_EQ(websocket_frame_parse(df, sizeof(df), &f), WEBSOCKET_FRAME_ERR_PROTOCOL);
}

TEST(test_build_null_payload_nonzero_len)
{
    uint8_t buf[32];
    size_t out;
    ASSERT_EQ(websocket_frame_build(buf, sizeof(buf), &out, true, WEBSOCKET_FRAME_OPCODE_TEXT,
                             NULL, NULL, 5), WEBSOCKET_FRAME_ERR_PARAM);
    ASSERT_EQ(websocket_frame_build(buf, sizeof(buf), &out, true, WEBSOCKET_FRAME_OPCODE_TEXT,
                             NULL, NULL, 0), WEBSOCKET_FRAME_OK);
}

TEST(test_parse_64bit_length_msb_set_rejected)
{
    /* RFC 6455 s5.2: MSB of 64-bit payload length MUST be 0 */
    uint8_t data[10] = {0x82, 127,
                         0x80, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x01};
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, sizeof(data), &f),
              WEBSOCKET_FRAME_ERR_PROTOCOL);
}

TEST(test_parse_fragmented_continuation)
{
    /* Non-FIN text frame followed by FIN continuation */
    uint8_t frag1[] = {0x01, 0x03, 'H', 'e', 'l'};
    websocket_frame_t f1;
    ASSERT_EQ(websocket_frame_parse(frag1, sizeof(frag1), &f1), WEBSOCKET_FRAME_OK);
    ASSERT_FALSE(f1.fin);
    ASSERT_EQ(f1.opcode, WEBSOCKET_FRAME_OPCODE_TEXT);

    uint8_t frag2[] = {0x80, 0x02, 'l', 'o'};
    websocket_frame_t f2;
    ASSERT_EQ(websocket_frame_parse(frag2, sizeof(frag2), &f2), WEBSOCKET_FRAME_OK);
    ASSERT_TRUE(f2.fin);
    ASSERT_EQ(f2.opcode, WEBSOCKET_FRAME_OPCODE_CONT);
}

TEST(test_parse_rsv_bits_rejected)
{
    /* RSV1 set (0x40) is a protocol error */
    uint8_t data[] = {0xC1, 0x00};  /* FIN=1, RSV1=1, opcode=text */
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, sizeof(data), &f),
              WEBSOCKET_FRAME_ERR_PROTOCOL);
}

TEST(test_parse_control_frame_fragmented_rejected)
{
    /* Control frames (opcode >= 0x08) MUST NOT be fragmented (FIN must be 1) */
    uint8_t data[] = {0x09, 0x00};  /* FIN=0, opcode=ping */
    websocket_frame_t f;
    ASSERT_EQ(websocket_frame_parse(data, sizeof(data), &f),
              WEBSOCKET_FRAME_ERR_PROTOCOL);
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
    RUN(test_parse_reserved_opcode);
    RUN(test_build_null_payload_nonzero_len);
    RUN(test_parse_64bit_length_msb_set_rejected);
    RUN(test_parse_fragmented_continuation);
    RUN(test_parse_rsv_bits_rejected);
    RUN(test_parse_control_frame_fragmented_rejected);
TEST_MAIN_END()
