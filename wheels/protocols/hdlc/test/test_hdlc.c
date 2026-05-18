// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hdlc.h"
#include <string.h>

/* ---------- FCS-16 tests ---------- */

TEST(test_fcs16_known_vector)
{
    /* Standard CRC-16-CCITT test: "123456789" -> 0x906E */
    const uint8_t data[] = "123456789";
    uint16_t fcs = hdlc_fcs16(data, 9);
    ASSERT_EQ_HEX(fcs, 0x906E);
}

TEST(test_fcs16_empty)
{
    uint8_t data[] = {0};
    uint16_t fcs = hdlc_fcs16(data, 0);
    /* FCS_INIT ^ 0xFFFF = 0xFFFF ^ 0xFFFF = 0x0000 */
    ASSERT_EQ_HEX(fcs, 0x0000);
}

TEST(test_fcs16_null)
{
    uint16_t fcs = hdlc_fcs16(NULL, 10);
    ASSERT_EQ_HEX(fcs, HDLC_FCS_INIT);
}

TEST(test_fcs16_single_byte)
{
    uint8_t data[] = {0x00};
    uint16_t fcs = hdlc_fcs16(data, 1);
    /* Must produce a non-trivial value */
    ASSERT_TRUE(fcs != 0x0000);
}

/* ---------- encode NULL params ---------- */

TEST(test_encode_null_params)
{
    size_t out_len;
    uint8_t dst[128];
    uint8_t data[] = {0x01};

    ASSERT_EQ(hdlc_encode(data, 1, NULL, 128, &out_len), HDLC_ERR_PARAM);
    ASSERT_EQ(hdlc_encode(data, 1, dst, 128, NULL), HDLC_ERR_PARAM);
    ASSERT_EQ(hdlc_encode(NULL, 1, dst, 128, &out_len), HDLC_ERR_PARAM);
    /* NULL data with len==0 is allowed */
    ASSERT_EQ(hdlc_encode(NULL, 0, dst, 128, &out_len), HDLC_OK);
}

/* ---------- encode/decode roundtrip ---------- */

TEST(test_roundtrip_empty)
{
    uint8_t encoded[64];
    size_t elen = 0;
    hdlc_err_t err = hdlc_encode(NULL, 0, encoded, sizeof(encoded), &elen);
    ASSERT_EQ(err, HDLC_OK);
    ASSERT_TRUE(elen > 0);
    ASSERT_EQ_HEX(encoded[0], HDLC_FLAG);
    ASSERT_EQ_HEX(encoded[elen - 1], HDLC_FLAG);

    /* Feed to parser */
    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[64];
    size_t dlen = 0;
    err = hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen);
    ASSERT_EQ(err, HDLC_OK);
    ASSERT_EQ(dlen, 0);
}

TEST(test_roundtrip_single_byte)
{
    uint8_t data[] = {0x42};
    uint8_t encoded[64];
    size_t elen = 0;
    ASSERT_EQ(hdlc_encode(data, 1, encoded, sizeof(encoded), &elen), HDLC_OK);

    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, 1);
    ASSERT_EQ_HEX(decoded[0], 0x42);
}

TEST(test_roundtrip_multi_byte)
{
    uint8_t data[] = {0x01, 0x03, 0x48, 0x65, 0x6C, 0x6C, 0x6F};
    uint8_t encoded[128];
    size_t elen = 0;
    ASSERT_EQ(hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen), HDLC_OK);

    uint8_t pbuf[128];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[128];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, sizeof(data));
    ASSERT_MEMEQ_N(decoded, data, sizeof(data));
}

TEST(test_roundtrip_with_flag_byte)
{
    /* Data containing 0x7E -- must be stuffed */
    uint8_t data[] = {0x7E, 0x41, 0x7E};
    uint8_t encoded[128];
    size_t elen = 0;
    ASSERT_EQ(hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen), HDLC_OK);

    /* Encoded should be longer due to stuffing */
    ASSERT_TRUE(elen > sizeof(data) + 4); /* flags(2) + fcs(2) + stuffing */

    uint8_t pbuf[128];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[128];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, sizeof(data));
    ASSERT_MEMEQ_N(decoded, data, sizeof(data));
}

TEST(test_roundtrip_with_escape_byte)
{
    /* Data containing 0x7D -- must be stuffed */
    uint8_t data[] = {0x7D, 0x42, 0x7D};
    uint8_t encoded[128];
    size_t elen = 0;
    ASSERT_EQ(hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen), HDLC_OK);

    uint8_t pbuf[128];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[128];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, sizeof(data));
    ASSERT_MEMEQ_N(decoded, data, sizeof(data));
}

TEST(test_roundtrip_both_special)
{
    /* Data with both 0x7E and 0x7D */
    uint8_t data[] = {0x7E, 0x7D, 0x7E, 0x7D, 0x00, 0xFF};
    uint8_t encoded[128];
    size_t elen = 0;
    ASSERT_EQ(hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen), HDLC_OK);

    uint8_t pbuf[128];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[128];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, sizeof(data));
    ASSERT_MEMEQ_N(decoded, data, sizeof(data));
}

TEST(test_roundtrip_all_flag_data)
{
    /* Worst case: all 0x7E bytes (maximum expansion) */
    uint8_t data[8];
    memset(data, 0x7E, sizeof(data));
    uint8_t encoded[64];
    size_t elen = 0;
    ASSERT_EQ(hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen), HDLC_OK);

    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, sizeof(data));
    ASSERT_MEMEQ_N(decoded, data, sizeof(data));
}

/* ---------- encode overflow ---------- */

TEST(test_encode_overflow)
{
    uint8_t data[] = {0x41, 0x42, 0x43};
    uint8_t small[4]; /* Way too small for flag+data+FCS+flag */
    size_t out_len = 0;
    ASSERT_EQ(hdlc_encode(data, sizeof(data), small, sizeof(small), &out_len),
              HDLC_ERR_OVERFLOW);
}

/* ---------- FCS verification ---------- */

TEST(test_fcs_known_good_frame)
{
    /* Encode known data, then verify roundtrip: decode should recover same FCS */
    uint8_t data[] = {0xFF, 0x03, 0xAA, 0xBB};
    uint16_t fcs = hdlc_fcs16(data, sizeof(data));

    /* Encode and decode via the full HDLC path to verify FCS integrity */
    uint8_t encoded[64];
    size_t elen = 0;
    ASSERT_EQ(hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen), HDLC_OK);

    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    /* Verify the FCS stored in parser matches computed FCS */
    ASSERT_TRUE(parser.idx >= 2);
    uint16_t stored_fcs = (uint16_t)(((uint16_t)parser.buf[parser.idx - 1] << 8) |
                          (uint16_t)parser.buf[parser.idx - 2]);
    ASSERT_EQ_HEX(stored_fcs, fcs);

    /* Verify get_frame succeeds (FCS check passes) */
    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_MEMEQ_N(decoded, data, sizeof(data));
}

TEST(test_fcs_mismatch)
{
    /* Encode a valid frame, then corrupt data in parser buffer */
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t encoded[64];
    size_t elen = 0;
    hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen);

    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    /* Corrupt one byte in parser's unstuffed buffer */
    if (parser.idx > 2) {
        parser.buf[0] ^= 0xFF;
    }

    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen),
              HDLC_ERR_FCS);
}

/* ---------- parser tests ---------- */

TEST(test_parser_null_params)
{
    uint8_t buf[64];
    hdlc_parser_t parser;

    ASSERT_EQ(hdlc_parser_init(NULL, buf, 64), HDLC_ERR_PARAM);
    ASSERT_EQ(hdlc_parser_init(&parser, NULL, 64), HDLC_ERR_PARAM);
    ASSERT_EQ(hdlc_parser_init(&parser, buf, 0), HDLC_ERR_PARAM);

    /* Feed with NULL parser */
    ASSERT_EQ((int)hdlc_parser_feed(NULL, 0x7E), (int)HDLC_STATE_ERROR);

    /* get_frame with NULL params */
    uint8_t out[64];
    size_t olen;
    ASSERT_EQ(hdlc_parser_get_frame(NULL, out, 64, &olen), HDLC_ERR_PARAM);
    ASSERT_EQ(hdlc_parser_get_frame(&parser, NULL, 64, &olen), HDLC_ERR_PARAM);
    ASSERT_EQ(hdlc_parser_get_frame(&parser, out, 64, NULL), HDLC_ERR_PARAM);
}

TEST(test_parser_garbage_before_flag)
{
    uint8_t data[] = {0xAA};
    uint8_t encoded[64];
    size_t elen = 0;
    hdlc_encode(data, 1, encoded, sizeof(encoded), &elen);

    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    /* Feed garbage first */
    hdlc_parser_feed(&parser, 0xDE);
    hdlc_parser_feed(&parser, 0xAD);
    hdlc_parser_feed(&parser, 0xBE);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_IDLE);

    /* Now feed the real frame */
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, 1);
    ASSERT_EQ_HEX(decoded[0], 0xAA);
}

TEST(test_parser_reset_mid_frame)
{
    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    /* Start a frame */
    hdlc_parser_feed(&parser, HDLC_FLAG);
    hdlc_parser_feed(&parser, 0x01);
    hdlc_parser_feed(&parser, 0x02);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_ACTIVE);

    /* Reset mid-frame */
    hdlc_parser_reset(&parser);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_IDLE);
    ASSERT_EQ(parser.idx, 0);

    /* Can start a new frame after reset */
    uint8_t data[] = {0xBB};
    uint8_t encoded[64];
    size_t elen = 0;
    hdlc_encode(data, 1, encoded, sizeof(encoded), &elen);

    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ_HEX(decoded[0], 0xBB);
}

TEST(test_parser_back_to_back_frames)
{
    uint8_t data1[] = {0x01, 0x02};
    uint8_t data2[] = {0x03, 0x04};
    uint8_t enc1[64], enc2[64];
    size_t len1 = 0, len2 = 0;
    hdlc_encode(data1, 2, enc1, sizeof(enc1), &len1);
    hdlc_encode(data2, 2, enc2, sizeof(enc2), &len2);

    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    /* Feed first frame */
    for (size_t i = 0; i < len1; i++) {
        hdlc_parser_feed(&parser, enc1[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, 2);
    ASSERT_MEMEQ_N(decoded, data1, 2);

    /* Reset and feed second frame */
    hdlc_parser_reset(&parser);
    for (size_t i = 0; i < len2; i++) {
        hdlc_parser_feed(&parser, enc2[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, 2);
    ASSERT_MEMEQ_N(decoded, data2, 2);
}

TEST(test_parser_max_length)
{
    /* Encode a frame that fills most of the buffer */
    uint8_t data[128];
    for (int i = 0; i < 128; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    uint8_t encoded[512];
    size_t elen = 0;
    ASSERT_EQ(hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen), HDLC_OK);

    uint8_t pbuf[256];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[256];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, sizeof(data));
    ASSERT_MEMEQ_N(decoded, data, sizeof(data));
}

TEST(test_parser_get_frame_not_complete)
{
    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    /* Parser is IDLE -- get_frame should fail */
    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen),
              HDLC_ERR_FORMAT);
}

TEST(test_parser_output_overflow)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t encoded[64];
    size_t elen = 0;
    hdlc_encode(data, sizeof(data), encoded, sizeof(encoded), &elen);

    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));
    for (size_t i = 0; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    /* Output buffer too small */
    uint8_t tiny[2];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, tiny, sizeof(tiny), &dlen),
              HDLC_ERR_OVERFLOW);
}

TEST(test_encode_stuffing_verified)
{
    /* Verify that 0x7E in data becomes 0x7D 0x5E and 0x7D becomes 0x7D 0x5D */
    uint8_t data[] = {0x7E, 0x7D};
    uint8_t encoded[64];
    size_t elen = 0;
    hdlc_encode(data, 2, encoded, sizeof(encoded), &elen);

    /* Check the encoded bytes (skip opening flag at [0]) */
    bool found_flag_stuff = false;
    bool found_esc_stuff = false;
    for (size_t i = 1; i < elen - 1; i++) {
        if (encoded[i] == HDLC_ESCAPE && i + 1 < elen - 1) {
            if (encoded[i + 1] == 0x5E) found_flag_stuff = true;
            if (encoded[i + 1] == 0x5D) found_esc_stuff = true;
        }
    }
    ASSERT_TRUE(found_flag_stuff);
    ASSERT_TRUE(found_esc_stuff);
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)HDLC_OK);
    ASSERT_EQ(-1, (int)HDLC_ERR_PARAM);
    ASSERT_EQ(-2, (int)HDLC_ERR_FORMAT);
    ASSERT_EQ(-3, (int)HDLC_ERR_OVERFLOW);
    ASSERT_EQ(-4, (int)HDLC_ERR_FCS);
}

TEST(test_parser_consecutive_flags)
{
    /* Multiple flags in a row should just keep parser in ACTIVE with idx=0 */
    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    hdlc_parser_feed(&parser, HDLC_FLAG);
    hdlc_parser_feed(&parser, HDLC_FLAG);
    hdlc_parser_feed(&parser, HDLC_FLAG);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_ACTIVE);
    ASSERT_EQ(parser.idx, 0);
}

TEST(test_parser_mini_fuzz)
{
    uint8_t pbuf[256];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    unsigned seed = 42;
    for (int i = 0; i < 2000; i++) {
        seed = seed * 1103515245u + 12345u;
        uint8_t byte = (uint8_t)(seed >> 16);
        hdlc_parser_feed(&parser, byte);
        /* Must not crash on any input */
    }
}

TEST(test_parser_escape_then_flag_aborts)
{
    /* Per RFC 1662: escape followed by flag is an abort.
     * Parser should discard the partial frame and restart. */
    uint8_t pbuf[64];
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    /* Start a frame with some data */
    hdlc_parser_feed(&parser, HDLC_FLAG);
    hdlc_parser_feed(&parser, 0x01);
    hdlc_parser_feed(&parser, 0x02);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_ACTIVE);

    /* Send escape */
    hdlc_parser_feed(&parser, HDLC_ESCAPE);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_ESCAPE);

    /* Send flag -- should abort the partial frame and restart */
    hdlc_parser_feed(&parser, HDLC_FLAG);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_ACTIVE);
    ASSERT_EQ(parser.idx, 0); /* Frame discarded */

    /* Now feed a complete valid frame -- should work */
    uint8_t data[] = {0xCC};
    uint8_t encoded[64];
    size_t elen = 0;
    hdlc_encode(data, 1, encoded, sizeof(encoded), &elen);

    /* Skip the opening flag since parser is already ACTIVE from the abort */
    for (size_t i = 1; i < elen; i++) {
        hdlc_parser_feed(&parser, encoded[i]);
    }
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_COMPLETE);

    uint8_t decoded[64];
    size_t dlen = 0;
    ASSERT_EQ(hdlc_parser_get_frame(&parser, decoded, sizeof(decoded), &dlen), HDLC_OK);
    ASSERT_EQ(dlen, 1);
    ASSERT_EQ_HEX(decoded[0], 0xCC);
}

TEST(test_parser_overflow_recovery)
{
    /* Parser buffer overflow should transition to ERROR,
     * then recovery on next flag */
    uint8_t pbuf[4]; /* tiny buffer */
    hdlc_parser_t parser;
    hdlc_parser_init(&parser, pbuf, sizeof(pbuf));

    hdlc_parser_feed(&parser, HDLC_FLAG);
    /* Feed more than 4 bytes to overflow */
    hdlc_parser_feed(&parser, 0x01);
    hdlc_parser_feed(&parser, 0x02);
    hdlc_parser_feed(&parser, 0x03);
    hdlc_parser_feed(&parser, 0x04);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_ACTIVE);

    /* This should overflow */
    hdlc_parser_feed(&parser, 0x05);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_ERROR);

    /* Recover with a flag */
    hdlc_parser_feed(&parser, HDLC_FLAG);
    ASSERT_EQ((int)parser.state, (int)HDLC_STATE_ACTIVE);
    ASSERT_EQ(parser.idx, 0);
}

TEST(test_parser_reset_null)
{
    /* Must not crash */
    hdlc_parser_reset(NULL);
}

TEST_MAIN("hdlc")
    RUN(test_fcs16_known_vector);
    RUN(test_fcs16_empty);
    RUN(test_fcs16_null);
    RUN(test_fcs16_single_byte);
    RUN(test_encode_null_params);
    RUN(test_roundtrip_empty);
    RUN(test_roundtrip_single_byte);
    RUN(test_roundtrip_multi_byte);
    RUN(test_roundtrip_with_flag_byte);
    RUN(test_roundtrip_with_escape_byte);
    RUN(test_roundtrip_both_special);
    RUN(test_roundtrip_all_flag_data);
    RUN(test_encode_overflow);
    RUN(test_fcs_known_good_frame);
    RUN(test_fcs_mismatch);
    RUN(test_parser_null_params);
    RUN(test_parser_garbage_before_flag);
    RUN(test_parser_reset_mid_frame);
    RUN(test_parser_back_to_back_frames);
    RUN(test_parser_max_length);
    RUN(test_parser_get_frame_not_complete);
    RUN(test_parser_output_overflow);
    RUN(test_encode_stuffing_verified);
    RUN(test_error_codes);
    RUN(test_parser_consecutive_flags);
    RUN(test_parser_mini_fuzz);
    RUN(test_parser_escape_then_flag_aborts);
    RUN(test_parser_overflow_recovery);
    RUN(test_parser_reset_null);
TEST_MAIN_END()
