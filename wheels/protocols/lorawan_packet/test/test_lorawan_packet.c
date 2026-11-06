// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "lorawan_packet.h"
#include <string.h>

/* ---------- build_data tests ---------- */

TEST(test_build_unconfirmed_uplink)
{
    uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_UNCONFIRMED_UP;
    frame.dev_addr = 0x01020304;
    frame.fcnt = 42;
    frame.fport = 1;
    frame.payload = payload;
    frame.payload_len = 3;
    frame.mic[0] = 0xDE; frame.mic[1] = 0xAD;
    frame.mic[2] = 0xBE; frame.mic[3] = 0xEF;

    uint8_t buf[64];
    int n = lorawan_build_data(buf, sizeof(buf), &frame);

    /* MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + FPort(1) + Payload(3) + MIC(4) = 16 */
    ASSERT_EQ(n, 16);

    /* MHDR: MType=0x02 => (0x02 << 5) | 0x00 = 0x40 */
    ASSERT_EQ_HEX(buf[0], 0x40);

    /* DevAddr little-endian: 0x01020304 => 04 03 02 01 */
    ASSERT_EQ_HEX(buf[1], 0x04);
    ASSERT_EQ_HEX(buf[2], 0x03);
    ASSERT_EQ_HEX(buf[3], 0x02);
    ASSERT_EQ_HEX(buf[4], 0x01);

    /* FCtrl: no flags */
    ASSERT_EQ_HEX(buf[5], 0x00);

    /* FCnt: 42 = 0x002A little-endian => 2A 00 */
    ASSERT_EQ_HEX(buf[6], 0x2A);
    ASSERT_EQ_HEX(buf[7], 0x00);

    /* FPort */
    ASSERT_EQ(buf[8], 1);

    /* Payload */
    ASSERT_EQ_HEX(buf[9],  0xAA);
    ASSERT_EQ_HEX(buf[10], 0xBB);
    ASSERT_EQ_HEX(buf[11], 0xCC);

    /* MIC */
    ASSERT_EQ_HEX(buf[12], 0xDE);
    ASSERT_EQ_HEX(buf[13], 0xAD);
    ASSERT_EQ_HEX(buf[14], 0xBE);
    ASSERT_EQ_HEX(buf[15], 0xEF);
}

TEST(test_build_confirmed_uplink_with_ack)
{
    uint8_t payload[] = {0x11};
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_CONFIRMED_UP;
    frame.dev_addr = 0xAABBCCDD;
    frame.fcnt = 1;
    frame.fport = 10;
    frame.ack = true;
    frame.payload = payload;
    frame.payload_len = 1;

    uint8_t buf[64];
    int n = lorawan_build_data(buf, sizeof(buf), &frame);

    /* MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + FPort(1) + Payload(1) + MIC(4) = 14 */
    ASSERT_EQ(n, 14);

    /* MHDR: MType=0x04 => (0x04 << 5) = 0x80 */
    ASSERT_EQ_HEX(buf[0], 0x80);

    /* FCtrl: ACK bit set => 0x20 */
    ASSERT_EQ_HEX(buf[5], 0x20);
}

TEST(test_build_adr_flag)
{
    uint8_t payload[] = {0x55};
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_UNCONFIRMED_UP;
    frame.dev_addr = 0x12345678;
    frame.fcnt = 0;
    frame.fport = 2;
    frame.adr = true;
    frame.payload = payload;
    frame.payload_len = 1;

    uint8_t buf[64];
    int n = lorawan_build_data(buf, sizeof(buf), &frame);
    ASSERT_TRUE(n > 0);

    /* FCtrl: ADR bit set => 0x80 */
    ASSERT_EQ_HEX(buf[5], 0x80);
}

TEST(test_build_empty_payload)
{
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_UNCONFIRMED_UP;
    frame.dev_addr = 0x11223344;
    frame.fcnt = 100;
    frame.payload = NULL;
    frame.payload_len = 0;

    uint8_t buf[64];
    int n = lorawan_build_data(buf, sizeof(buf), &frame);

    /* MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + MIC(4) = 12 (no FPort) */
    ASSERT_EQ(n, 12);
}

TEST(test_build_buffer_too_small)
{
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_UNCONFIRMED_UP;
    frame.dev_addr = 0x01020304;
    frame.fcnt = 1;
    frame.fport = 1;
    frame.payload = payload;
    frame.payload_len = 5;

    /* Need 17 bytes, provide only 10 */
    uint8_t buf[10];
    int n = lorawan_build_data(buf, 10, &frame);
    ASSERT_EQ(n, LORAWAN_ERR_OVERFLOW);
}

TEST(test_build_null_params)
{
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    uint8_t buf[64];

    ASSERT_EQ(lorawan_build_data(NULL, 64, &frame), LORAWAN_ERR_PARAM);
    ASSERT_EQ(lorawan_build_data(buf, 64, NULL), LORAWAN_ERR_PARAM);
}

/* ---------- parse_data tests ---------- */

TEST(test_parse_unconfirmed_downlink)
{
    /* Build a frame, then parse it back */
    uint8_t payload[] = {0xDE, 0xAD};
    lorawan_frame_t build_frame;
    memset(&build_frame, 0, sizeof(build_frame));
    build_frame.mtype = LORAWAN_UNCONFIRMED_DOWN;
    build_frame.dev_addr = 0xCAFEBABE;
    build_frame.fcnt = 256;
    build_frame.fport = 3;
    build_frame.payload = payload;
    build_frame.payload_len = 2;
    build_frame.mic[0] = 0x11; build_frame.mic[1] = 0x22;
    build_frame.mic[2] = 0x33; build_frame.mic[3] = 0x44;

    uint8_t buf[64];
    int n = lorawan_build_data(buf, sizeof(buf), &build_frame);
    ASSERT_TRUE(n > 0);

    lorawan_frame_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    lorawan_err_t err = lorawan_parse_data(buf, (uint16_t)n, &parsed);
    ASSERT_EQ(err, LORAWAN_OK);
    ASSERT_EQ(parsed.mtype, LORAWAN_UNCONFIRMED_DOWN);
    ASSERT_EQ_HEX(parsed.dev_addr, 0xCAFEBABE);
    ASSERT_EQ(parsed.fcnt, 256);
    ASSERT_EQ(parsed.fport, 3);
    ASSERT_EQ(parsed.payload_len, 2);
    ASSERT_EQ_HEX(parsed.payload[0], 0xDE);
    ASSERT_EQ_HEX(parsed.payload[1], 0xAD);
    ASSERT_EQ_HEX(parsed.mic[0], 0x11);
    ASSERT_EQ_HEX(parsed.mic[3], 0x44);
}

TEST(test_parse_short_buffer)
{
    uint8_t buf[] = {0x40, 0x01, 0x02};
    lorawan_frame_t frame;
    ASSERT_EQ(lorawan_parse_data(buf, 3, &frame), LORAWAN_ERR_SIZE);
}

TEST(test_parse_null_params)
{
    uint8_t buf[16];
    lorawan_frame_t frame;
    ASSERT_EQ(lorawan_parse_data(NULL, 16, &frame), LORAWAN_ERR_PARAM);
    ASSERT_EQ(lorawan_parse_data(buf, 16, NULL), LORAWAN_ERR_PARAM);
}

TEST(test_parse_with_fopts)
{
    /*
     * Build a raw frame with FOptsLen=2 manually:
     *   MHDR(1) + DevAddr(4) + FCtrl(1, FOptsLen=2) + FCnt(2) + FOpts(2)
     *   + FPort(1) + Payload(1) + MIC(4) = 16
     */
    uint8_t raw[16];
    int pos = 0;

    /* MHDR: UnconfirmedUp */
    raw[pos++] = 0x40;

    /* DevAddr: 0x01020304 little-endian */
    raw[pos++] = 0x04; raw[pos++] = 0x03;
    raw[pos++] = 0x02; raw[pos++] = 0x01;

    /* FCtrl: FOptsLen=2 */
    raw[pos++] = 0x02;

    /* FCnt: 5 little-endian */
    raw[pos++] = 0x05; raw[pos++] = 0x00;

    /* FOpts: 2 bytes of MAC commands */
    raw[pos++] = 0xAA; raw[pos++] = 0xBB;

    /* FPort */
    raw[pos++] = 1;

    /* Payload: 1 byte */
    raw[pos++] = 0xFF;

    /* MIC */
    raw[pos++] = 0x00; raw[pos++] = 0x00;
    raw[pos++] = 0x00; raw[pos++] = 0x00;

    lorawan_frame_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    lorawan_err_t err = lorawan_parse_data(raw, (uint16_t)pos, &parsed);
    ASSERT_EQ(err, LORAWAN_OK);
    ASSERT_EQ_HEX(parsed.dev_addr, 0x01020304);
    ASSERT_EQ(parsed.fcnt, 5);
    ASSERT_EQ(parsed.fport, 1);
    ASSERT_EQ(parsed.payload_len, 1);
    ASSERT_EQ_HEX(parsed.payload[0], 0xFF);
}

TEST(test_parse_empty_payload)
{
    /*
     * Frame with no payload (no FPort):
     *   MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) + MIC(4) = 12
     */
    lorawan_frame_t build_frame;
    memset(&build_frame, 0, sizeof(build_frame));
    build_frame.mtype = LORAWAN_UNCONFIRMED_UP;
    build_frame.dev_addr = 0xDEADBEEF;
    build_frame.fcnt = 0;
    build_frame.payload = NULL;
    build_frame.payload_len = 0;

    uint8_t buf[64];
    int n = lorawan_build_data(buf, sizeof(buf), &build_frame);
    ASSERT_EQ(n, 12);

    lorawan_frame_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    lorawan_err_t err = lorawan_parse_data(buf, (uint16_t)n, &parsed);
    ASSERT_EQ(err, LORAWAN_OK);
    ASSERT_EQ(parsed.payload_len, 0);
    ASSERT_TRUE(parsed.payload == NULL);
    ASSERT_EQ(parsed.fport, 0);
}

/* ---------- join request tests ---------- */

TEST(test_join_request_roundtrip)
{
    uint8_t app_eui[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t dev_eui[8] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8};
    uint8_t dev_nonce[2] = {0xBE, 0xEF};

    uint8_t buf[32];
    int n = lorawan_build_join_request(buf, sizeof(buf), app_eui, dev_eui, dev_nonce);
    ASSERT_EQ(n, 19);

    /* MHDR: JoinRequest MType=0x00 => 0x00 */
    ASSERT_EQ_HEX(buf[0], 0x00);

    uint8_t parsed_app[8], parsed_dev[8], parsed_nonce[2];
    lorawan_err_t err = lorawan_parse_join_request(buf, (uint16_t)n,
                                                    parsed_app, parsed_dev,
                                                    parsed_nonce);
    ASSERT_EQ(err, LORAWAN_OK);
    ASSERT_TRUE(memcmp(parsed_app, app_eui, 8) == 0);
    ASSERT_TRUE(memcmp(parsed_dev, dev_eui, 8) == 0);
    ASSERT_TRUE(memcmp(parsed_nonce, dev_nonce, 2) == 0);
}

TEST(test_join_request_null_params)
{
    uint8_t buf[32], eui[8] = {0}, nonce[2] = {0};
    ASSERT_EQ(lorawan_build_join_request(NULL, 32, eui, eui, nonce), LORAWAN_ERR_PARAM);
    ASSERT_EQ(lorawan_build_join_request(buf, 32, NULL, eui, nonce), LORAWAN_ERR_PARAM);
    ASSERT_EQ(lorawan_build_join_request(buf, 32, eui, NULL, nonce), LORAWAN_ERR_PARAM);
    ASSERT_EQ(lorawan_build_join_request(buf, 32, eui, eui, NULL), LORAWAN_ERR_PARAM);

    ASSERT_EQ(lorawan_parse_join_request(NULL, 19, eui, eui, nonce), LORAWAN_ERR_PARAM);
    ASSERT_EQ(lorawan_parse_join_request(buf, 19, NULL, eui, nonce), LORAWAN_ERR_PARAM);
}

TEST(test_join_request_buffer_too_small)
{
    uint8_t eui[8] = {0}, nonce[2] = {0};
    uint8_t buf[10];
    ASSERT_EQ(lorawan_build_join_request(buf, 10, eui, eui, nonce), LORAWAN_ERR_OVERFLOW);
}

TEST(test_join_request_parse_short)
{
    uint8_t buf[10] = {0x00}; /* JoinRequest MHDR but too short */
    uint8_t eui[8], nonce[2];
    ASSERT_EQ(lorawan_parse_join_request(buf, 10, eui, eui, nonce), LORAWAN_ERR_SIZE);
}

/* ---------- helper tests ---------- */

TEST(test_mtype_extraction)
{
    /* UnconfirmedUp: MType=0x02 => MHDR = 0x40 */
    ASSERT_EQ(lorawan_get_mtype(0x40), LORAWAN_UNCONFIRMED_UP);
    /* ConfirmedDown: MType=0x05 => MHDR = 0xA0 */
    ASSERT_EQ(lorawan_get_mtype(0xA0), LORAWAN_CONFIRMED_DOWN);
    /* JoinRequest: MType=0x00 => MHDR = 0x00 */
    ASSERT_EQ(lorawan_get_mtype(0x00), LORAWAN_JOIN_REQUEST);
}

TEST(test_devaddr_encoding_le)
{
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_UNCONFIRMED_UP;
    frame.dev_addr = 0xAABBCCDD;
    frame.fcnt = 0;
    frame.payload = NULL;
    frame.payload_len = 0;

    uint8_t buf[64];
    int n = lorawan_build_data(buf, sizeof(buf), &frame);
    ASSERT_TRUE(n > 0);

    /* DevAddr at offset 1, little-endian: DD CC BB AA */
    ASSERT_EQ_HEX(buf[1], 0xDD);
    ASSERT_EQ_HEX(buf[2], 0xCC);
    ASSERT_EQ_HEX(buf[3], 0xBB);
    ASSERT_EQ_HEX(buf[4], 0xAA);
}

TEST(test_fcnt_encoding_le)
{
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_UNCONFIRMED_UP;
    frame.dev_addr = 0;
    frame.fcnt = 0x1234;
    frame.payload = NULL;
    frame.payload_len = 0;

    uint8_t buf[64];
    int n = lorawan_build_data(buf, sizeof(buf), &frame);
    ASSERT_TRUE(n > 0);

    /* FCnt at offset 6, little-endian: 34 12 */
    ASSERT_EQ_HEX(buf[6], 0x34);
    ASSERT_EQ_HEX(buf[7], 0x12);
}

/* ---------- payload length limits ---------- */

/* LoRaWAN Regional Parameters RP002-1.0.3: the largest MACPayload of any
 * region is M = 250, which leaves N = M - 8 = 242 bytes of FRMPayload when
 * FOpts is empty. With MHDR(1) and MIC(4) the frame is 255 bytes. */
static uint8_t big_payload[65535];

TEST(test_build_refuses_a_length_that_wraps_16_bits)
{
    /* The other 13 frame bytes take these to 65536, 65543 and 65548. */
    static const uint16_t wrapping[] = { 65523u, 65530u, 65535u };

    for (size_t i = 0; i < sizeof(wrapping) / sizeof(wrapping[0]); i++) {
        lorawan_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.mtype = LORAWAN_UNCONFIRMED_UP;
        frame.payload = big_payload;
        frame.payload_len = wrapping[i];

        uint8_t buf[64];
        memset(buf, 0x5A, sizeof(buf));
        ASSERT_EQ(lorawan_build_data(buf, sizeof(buf), &frame), LORAWAN_ERR_SIZE);
        for (size_t j = 0; j < sizeof(buf); j++)
            ASSERT_EQ_HEX(buf[j], 0x5A);
    }
}

TEST(test_build_takes_the_largest_lorawan_payload)
{
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_UNCONFIRMED_UP;
    frame.payload = big_payload;
    frame.payload_len = 242;

    uint8_t buf[255];
    ASSERT_EQ(lorawan_build_data(buf, 255, &frame), 255);
    ASSERT_EQ(lorawan_build_data(buf, 254, &frame), LORAWAN_ERR_OVERFLOW);
}

TEST(test_build_refuses_a_payload_above_the_lorawan_maximum)
{
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = LORAWAN_UNCONFIRMED_UP;
    frame.payload = big_payload;
    frame.payload_len = 243;

    uint8_t buf[300];
    ASSERT_EQ(lorawan_build_data(buf, sizeof(buf), &frame), LORAWAN_ERR_SIZE);
}

TEST_MAIN("lorawan_packet")
    RUN(test_build_unconfirmed_uplink);
    RUN(test_build_confirmed_uplink_with_ack);
    RUN(test_build_adr_flag);
    RUN(test_build_empty_payload);
    RUN(test_build_buffer_too_small);
    RUN(test_build_null_params);
    RUN(test_parse_unconfirmed_downlink);
    RUN(test_parse_short_buffer);
    RUN(test_parse_null_params);
    RUN(test_parse_with_fopts);
    RUN(test_parse_empty_payload);
    RUN(test_join_request_roundtrip);
    RUN(test_join_request_null_params);
    RUN(test_join_request_buffer_too_small);
    RUN(test_join_request_parse_short);
    RUN(test_mtype_extraction);
    RUN(test_devaddr_encoding_le);
    RUN(test_fcnt_encoding_le);
    RUN(test_build_refuses_a_length_that_wraps_16_bits);
    RUN(test_build_takes_the_largest_lorawan_payload);
    RUN(test_build_refuses_a_payload_above_the_lorawan_maximum);
TEST_MAIN_END()
