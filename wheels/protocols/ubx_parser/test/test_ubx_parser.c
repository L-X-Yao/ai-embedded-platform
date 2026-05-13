// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "ubx_parser.h"
#include <string.h>

/* ---------- checksum tests ---------- */

TEST(test_checksum_basic)
{
    /* Known vector: class=0x01, id=0x07, length=0x0000 (no payload)
     * Data to checksum: 01 07 00 00
     * Fletcher-8: a=01, b=01 -> a=08, b=09 -> a=08, b=11 -> a=08, b=19
     *             a=1+7+0+0=8, b=1+8+8+8=25  -> ck_a=0x08, ck_b=0x19 */
    uint8_t data[] = {0x01, 0x07, 0x00, 0x00};
    uint8_t ck_a, ck_b;
    ubx_parser_checksum(data, sizeof(data), &ck_a, &ck_b);
    ASSERT_EQ_HEX(ck_a, 0x08);
    ASSERT_EQ_HEX(ck_b, 0x19);
}

TEST(test_checksum_nav_pvt_payload)
{
    /* class=0x01 id=0x07 len=0x0400 (4 bytes) payload: AA BB CC DD */
    uint8_t data[] = {0x01, 0x07, 0x04, 0x00, 0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t ck_a, ck_b;
    ubx_parser_checksum(data, sizeof(data), &ck_a, &ck_b);
    /* Manually computed Fletcher-8:
     * i=0: a=0x01, b=0x01
     * i=1: a=0x08, b=0x09
     * i=2: a=0x0C, b=0x15
     * i=3: a=0x0C, b=0x21
     * i=4: a=0xB6, b=0xD7
     * i=5: a=0x71, b=0x48   (0x71+0x48 mod 256)
     * i=6: a=0x3D, b=0x85
     * i=7: a=0x1A, b=0x9F */
    ASSERT_EQ_HEX(ck_a, 0x1A);
    ASSERT_EQ_HEX(ck_b, 0x9F);
}

TEST(test_checksum_null_safety)
{
    uint8_t ck_a = 0xFF, ck_b = 0xFF;
    ubx_parser_checksum(NULL, 5, &ck_a, &ck_b);
    /* Should not crash; values unchanged */
    ASSERT_EQ_HEX(ck_a, 0xFF);

    uint8_t data[] = {0x01};
    ubx_parser_checksum(data, 1, NULL, &ck_b);
    ubx_parser_checksum(data, 1, &ck_a, NULL);
}

TEST(test_checksum_empty)
{
    uint8_t data[] = {0x00};
    uint8_t ck_a, ck_b;
    ubx_parser_checksum(data, 0, &ck_a, &ck_b);
    ASSERT_EQ_HEX(ck_a, 0x00);
    ASSERT_EQ_HEX(ck_b, 0x00);
}

/* ---------- build tests ---------- */

TEST(test_build_no_payload)
{
    uint8_t buf[64];
    int n = ubx_parser_build(buf, sizeof(buf), UBX_CLASS_NAV, UBX_NAV_PVT, NULL, 0);
    ASSERT_EQ(n, UBX_OVERHEAD); /* 8 bytes total */
    ASSERT_EQ_HEX(buf[0], UBX_SYNC1);
    ASSERT_EQ_HEX(buf[1], UBX_SYNC2);
    ASSERT_EQ_HEX(buf[2], 0x01); /* class NAV */
    ASSERT_EQ_HEX(buf[3], 0x07); /* id PVT */
    ASSERT_EQ_HEX(buf[4], 0x00); /* length low */
    ASSERT_EQ_HEX(buf[5], 0x00); /* length high */
    /* Checksum over 01 07 00 00 */
    ASSERT_EQ_HEX(buf[6], 0x08);
    ASSERT_EQ_HEX(buf[7], 0x19);
}

TEST(test_build_with_payload)
{
    uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t buf[64];
    int n = ubx_parser_build(buf, sizeof(buf), UBX_CLASS_NAV, UBX_NAV_PVT,
                             payload, 4);
    ASSERT_EQ(n, UBX_OVERHEAD + 4); /* 12 bytes */
    ASSERT_EQ_HEX(buf[0], UBX_SYNC1);
    ASSERT_EQ_HEX(buf[1], UBX_SYNC2);
    ASSERT_EQ_HEX(buf[4], 0x04); /* length low */
    ASSERT_EQ_HEX(buf[5], 0x00); /* length high */
    /* Payload at offset 6 */
    ASSERT_EQ_HEX(buf[6], 0xAA);
    ASSERT_EQ_HEX(buf[7], 0xBB);
    ASSERT_EQ_HEX(buf[8], 0xCC);
    ASSERT_EQ_HEX(buf[9], 0xDD);
    /* Checksum */
    ASSERT_EQ_HEX(buf[10], 0x1A);
    ASSERT_EQ_HEX(buf[11], 0x9F);
}

TEST(test_build_buffer_too_small)
{
    uint8_t buf[4];
    int n = ubx_parser_build(buf, sizeof(buf), 0x01, 0x07, NULL, 0);
    ASSERT_EQ(n, -2);
}

TEST(test_build_null_params)
{
    ASSERT_EQ(ubx_parser_build(NULL, 64, 0x01, 0x07, NULL, 0), -1);

    uint8_t buf[64];
    uint8_t payload[] = {0x01};
    /* payload_len > 0 but payload NULL */
    ASSERT_EQ(ubx_parser_build(buf, sizeof(buf), 0x01, 0x07, NULL, 1), -1);
    /* payload != NULL, payload_len == 0 is fine */
    int n = ubx_parser_build(buf, sizeof(buf), 0x01, 0x07, payload, 0);
    ASSERT_EQ(n, UBX_OVERHEAD);
}

TEST(test_build_ack)
{
    /* ACK-ACK: class=0x05 id=0x01, payload: clsID=0x06 msgID=0x01 */
    uint8_t payload[] = {0x06, 0x01};
    uint8_t buf[64];
    int n = ubx_parser_build(buf, sizeof(buf), UBX_CLASS_ACK, UBX_ACK_ACK,
                             payload, 2);
    ASSERT_EQ(n, UBX_OVERHEAD + 2);
    ASSERT_EQ_HEX(buf[2], UBX_CLASS_ACK);
    ASSERT_EQ_HEX(buf[3], UBX_ACK_ACK);
}

/* ---------- init/feed/get_msg tests ---------- */

TEST(test_init_null)
{
    uint8_t buf[64];
    ASSERT_EQ(ubx_parser_init(NULL, buf, sizeof(buf)), UBX_PARSER_ERR_PARAM);

    ubx_parser_t ctx;
    ASSERT_EQ(ubx_parser_init(&ctx, NULL, 64), UBX_PARSER_ERR_PARAM);
}

TEST(test_init_too_small)
{
    ubx_parser_t ctx;
    uint8_t buf[4];
    ASSERT_EQ(ubx_parser_init(&ctx, buf, sizeof(buf)), UBX_PARSER_ERR_SHORT);
}

TEST(test_feed_complete_no_payload)
{
    uint8_t fbuf[64];
    ubx_parser_t ctx;
    ASSERT_EQ(ubx_parser_init(&ctx, fbuf, sizeof(fbuf)), UBX_PARSER_OK);

    /* Build a valid frame */
    uint8_t frame[64];
    int flen = ubx_parser_build(frame, sizeof(frame), UBX_CLASS_NAV, UBX_NAV_PVT,
                                NULL, 0);
    ASSERT_TRUE(flen > 0);

    /* Feed byte by byte */
    ubx_feed_status_t st = UBX_FEED_IDLE;
    for (int i = 0; i < flen; i++) {
        st = ubx_parser_feed(&ctx, frame[i]);
    }
    ASSERT_EQ(st, UBX_FEED_COMPLETE);

    /* Extract message */
    ubx_msg_t msg;
    ASSERT_EQ(ubx_parser_get_msg(&ctx, &msg), UBX_PARSER_OK);
    ASSERT_EQ_HEX(msg.msg_class, UBX_CLASS_NAV);
    ASSERT_EQ_HEX(msg.msg_id, UBX_NAV_PVT);
    ASSERT_EQ(msg.payload_len, 0);
    ASSERT_TRUE(msg.payload == NULL);
}

TEST(test_feed_complete_with_payload)
{
    uint8_t fbuf[64];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, fbuf, sizeof(fbuf));

    uint8_t payload[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t frame[64];
    int flen = ubx_parser_build(frame, sizeof(frame), UBX_CLASS_CFG, 0x01,
                                payload, 5);
    ASSERT_TRUE(flen > 0);

    ubx_feed_status_t st = UBX_FEED_IDLE;
    for (int i = 0; i < flen; i++) {
        st = ubx_parser_feed(&ctx, frame[i]);
    }
    ASSERT_EQ(st, UBX_FEED_COMPLETE);

    ubx_msg_t msg;
    ASSERT_EQ(ubx_parser_get_msg(&ctx, &msg), UBX_PARSER_OK);
    ASSERT_EQ_HEX(msg.msg_class, UBX_CLASS_CFG);
    ASSERT_EQ_HEX(msg.msg_id, 0x01);
    ASSERT_EQ(msg.payload_len, 5);
    ASSERT_NOT_NULL(msg.payload);
    ASSERT_EQ_HEX(msg.payload[0], 0x11);
    ASSERT_EQ_HEX(msg.payload[4], 0x55);
}

TEST(test_feed_bad_checksum)
{
    uint8_t fbuf[64];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, fbuf, sizeof(fbuf));

    uint8_t frame[64];
    int flen = ubx_parser_build(frame, sizeof(frame), 0x01, 0x07, NULL, 0);
    ASSERT_TRUE(flen > 0);

    /* Corrupt checksum */
    frame[flen - 1] ^= 0xFF;

    ubx_feed_status_t st = UBX_FEED_IDLE;
    for (int i = 0; i < flen; i++) {
        st = ubx_parser_feed(&ctx, frame[i]);
    }
    ASSERT_EQ(st, UBX_FEED_ERROR);
}

TEST(test_feed_garbage_before_sync)
{
    uint8_t fbuf[64];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, fbuf, sizeof(fbuf));

    /* Feed some garbage */
    ubx_parser_feed(&ctx, 0x00);
    ubx_parser_feed(&ctx, 0xFF);
    ubx_parser_feed(&ctx, 0x42);

    /* Now feed a valid frame */
    uint8_t frame[64];
    int flen = ubx_parser_build(frame, sizeof(frame), 0x01, 0x07, NULL, 0);

    ubx_feed_status_t st = UBX_FEED_IDLE;
    for (int i = 0; i < flen; i++) {
        st = ubx_parser_feed(&ctx, frame[i]);
    }
    ASSERT_EQ(st, UBX_FEED_COMPLETE);
}

TEST(test_feed_reset_and_reuse)
{
    uint8_t fbuf[64];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, fbuf, sizeof(fbuf));

    uint8_t frame[64];
    int flen = ubx_parser_build(frame, sizeof(frame), 0x01, 0x07, NULL, 0);

    for (int i = 0; i < flen; i++)
        ubx_parser_feed(&ctx, frame[i]);

    /* Reset and parse again */
    ubx_parser_reset(&ctx);

    uint8_t payload[] = {0xDE, 0xAD};
    flen = ubx_parser_build(frame, sizeof(frame), 0x05, 0x01, payload, 2);

    ubx_feed_status_t st = UBX_FEED_IDLE;
    for (int i = 0; i < flen; i++)
        st = ubx_parser_feed(&ctx, frame[i]);
    ASSERT_EQ(st, UBX_FEED_COMPLETE);

    ubx_msg_t msg;
    ubx_parser_get_msg(&ctx, &msg);
    ASSERT_EQ_HEX(msg.msg_class, 0x05);
    ASSERT_EQ(msg.payload_len, 2);
}

TEST(test_feed_null_ctx)
{
    ASSERT_EQ(ubx_parser_feed(NULL, 0xB5), UBX_FEED_ERROR);
}

TEST(test_get_msg_not_complete)
{
    uint8_t fbuf[64];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, fbuf, sizeof(fbuf));

    ubx_msg_t msg;
    ASSERT_EQ(ubx_parser_get_msg(&ctx, &msg), UBX_PARSER_ERR_PARAM);
}

TEST(test_get_msg_null)
{
    ASSERT_EQ(ubx_parser_get_msg(NULL, NULL), UBX_PARSER_ERR_PARAM);
}

TEST(test_reset_null)
{
    ubx_parser_reset(NULL); /* must not crash */
}

/* ---------- round-trip test ---------- */

TEST(test_build_feed_roundtrip)
{
    /* Build with various payload sizes */
    uint8_t frame_buf[300];
    uint8_t parse_buf[300];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, parse_buf, sizeof(parse_buf));

    uint8_t payload[128];
    for (int plen = 0; plen < 128; plen++) {
        for (int j = 0; j < plen; j++)
            payload[j] = (uint8_t)(j & 0xFF);

        int flen = ubx_parser_build(frame_buf, sizeof(frame_buf),
                                    0x01, 0x07, payload, (uint16_t)plen);
        ASSERT_TRUE(flen > 0);

        ubx_parser_reset(&ctx);
        ubx_feed_status_t st = UBX_FEED_IDLE;
        for (int i = 0; i < flen; i++)
            st = ubx_parser_feed(&ctx, frame_buf[i]);
        ASSERT_EQ(st, UBX_FEED_COMPLETE);

        ubx_msg_t msg;
        ubx_parser_get_msg(&ctx, &msg);
        ASSERT_EQ(msg.payload_len, plen);
        if (plen > 0) {
            ASSERT_MEMEQ_N(msg.payload, payload, (size_t)plen);
        }
    }
}

/* ---------- known byte vector test ---------- */

TEST(test_known_byte_sequence)
{
    /* Manual construction of NAV-PVT with 4 bytes payload:
     * B5 62 01 07 04 00 DE AD BE EF ck_a ck_b
     * Checksum over: 01 07 04 00 DE AD BE EF */
    uint8_t ckdata[] = {0x01, 0x07, 0x04, 0x00, 0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t ck_a, ck_b;
    ubx_parser_checksum(ckdata, sizeof(ckdata), &ck_a, &ck_b);

    uint8_t frame[12] = {0xB5, 0x62, 0x01, 0x07, 0x04, 0x00,
                          0xDE, 0xAD, 0xBE, 0xEF, ck_a, ck_b};

    uint8_t pbuf[64];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, pbuf, sizeof(pbuf));

    ubx_feed_status_t st = UBX_FEED_IDLE;
    for (int i = 0; i < 12; i++)
        st = ubx_parser_feed(&ctx, frame[i]);
    ASSERT_EQ(st, UBX_FEED_COMPLETE);

    ubx_msg_t msg;
    ubx_parser_get_msg(&ctx, &msg);
    ASSERT_EQ_HEX(msg.msg_class, 0x01);
    ASSERT_EQ_HEX(msg.msg_id, 0x07);
    ASSERT_EQ(msg.payload_len, 4);
    ASSERT_EQ_HEX(msg.payload[0], 0xDE);
    ASSERT_EQ_HEX(msg.payload[3], 0xEF);
}

TEST(test_feed_resync_on_double_sync1)
{
    /* Feed sync1 twice -- second one should restart */
    uint8_t fbuf[64];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, fbuf, sizeof(fbuf));

    ubx_parser_feed(&ctx, UBX_SYNC1);
    /* Another sync1 instead of sync2 */
    ubx_parser_feed(&ctx, UBX_SYNC1);
    /* Now feed sync2 + valid frame rest */
    uint8_t frame[64];
    int flen = ubx_parser_build(frame, sizeof(frame), 0x01, 0x07, NULL, 0);
    /* Feed from sync2 onward (skip first sync1 which we already fed) */
    ubx_feed_status_t st = UBX_FEED_IDLE;
    for (int i = 1; i < flen; i++)
        st = ubx_parser_feed(&ctx, frame[i]);
    ASSERT_EQ(st, UBX_FEED_COMPLETE);
}

TEST(test_feed_overflow_protection)
{
    /* Use a very small buffer that cannot hold a large payload */
    uint8_t small_buf[12]; /* can hold 8 overhead + 4 payload max */
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, small_buf, sizeof(small_buf));

    /* Build a frame with payload too large for the buffer */
    uint8_t payload[10] = {0};
    uint8_t frame[64];
    int flen = ubx_parser_build(frame, sizeof(frame), 0x01, 0x07, payload, 10);
    ASSERT_TRUE(flen > 0);

    ubx_feed_status_t st = UBX_FEED_IDLE;
    for (int i = 0; i < flen; i++) {
        st = ubx_parser_feed(&ctx, frame[i]);
        if (st == UBX_FEED_ERROR) break;
    }
    ASSERT_EQ(st, UBX_FEED_ERROR);
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)UBX_PARSER_OK);
    ASSERT_EQ(-1, (int)UBX_PARSER_ERR_PARAM);
    ASSERT_EQ(-2, (int)UBX_PARSER_ERR_CHECKSUM);
    ASSERT_EQ(-3, (int)UBX_PARSER_ERR_SHORT);
    ASSERT_EQ(-4, (int)UBX_PARSER_ERR_OVERFLOW);
}

TEST(test_mini_fuzz)
{
    uint8_t fbuf[300];
    ubx_parser_t ctx;
    ubx_parser_init(&ctx, fbuf, sizeof(fbuf));

    unsigned seed = 42;
    for (int i = 0; i < 2000; i++) {
        seed = seed * 1103515245u + 12345u;
        uint8_t byte = (uint8_t)(seed >> 16);
        ubx_feed_status_t st = ubx_parser_feed(&ctx, byte);
        if (st == UBX_FEED_COMPLETE || st == UBX_FEED_ERROR)
            ubx_parser_reset(&ctx);
    }
    /* Must not crash */
}

TEST_MAIN("ubx_parser")
    RUN(test_checksum_basic);
    RUN(test_checksum_nav_pvt_payload);
    RUN(test_checksum_null_safety);
    RUN(test_checksum_empty);
    RUN(test_build_no_payload);
    RUN(test_build_with_payload);
    RUN(test_build_buffer_too_small);
    RUN(test_build_null_params);
    RUN(test_build_ack);
    RUN(test_init_null);
    RUN(test_init_too_small);
    RUN(test_feed_complete_no_payload);
    RUN(test_feed_complete_with_payload);
    RUN(test_feed_bad_checksum);
    RUN(test_feed_garbage_before_sync);
    RUN(test_feed_reset_and_reuse);
    RUN(test_feed_null_ctx);
    RUN(test_get_msg_not_complete);
    RUN(test_get_msg_null);
    RUN(test_reset_null);
    RUN(test_build_feed_roundtrip);
    RUN(test_known_byte_sequence);
    RUN(test_feed_resync_on_double_sync1);
    RUN(test_feed_overflow_protection);
    RUN(test_error_codes);
    RUN(test_mini_fuzz);
TEST_MAIN_END()
