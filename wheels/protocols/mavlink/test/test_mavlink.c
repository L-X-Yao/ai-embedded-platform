// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mavlink.h"
#include <string.h>

static uint16_t build_heartbeat_frame(uint8_t *buf, uint16_t buf_len,
                                      uint8_t seq, uint8_t sysid, uint8_t compid)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len    = 9;
    msg.seq    = seq;
    msg.sysid  = sysid;
    msg.compid = compid;
    msg.msgid  = MAVLINK_MSG_ID_HEARTBEAT;
    msg.payload[0] = 0x00;
    msg.payload[1] = 0x00;
    msg.payload[2] = 0x00;
    msg.payload[3] = 0x00;
    msg.payload[4] = 6;
    msg.payload[5] = 8;
    msg.payload[6] = 0;
    msg.payload[7] = 4;
    msg.payload[8] = 3;
    uint16_t out_len = 0;
    mavlink_serialize(&msg, buf, buf_len, &out_len, 50);
    return out_len;
}

static mavlink_parse_state_t parse_frame(mavlink_parser_t *p,
                                         const uint8_t *buf, uint16_t len)
{
    mavlink_parse_state_t st = MAVLINK_PARSE_IDLE;
    for (uint16_t i = 0; i < len; i++) {
        st = mavlink_parse_byte(p, buf[i]);
    }
    return st;
}

TEST(test_parser_init)
{
    mavlink_parser_t p;
    ASSERT_EQ(mavlink_parser_init(&p), MAVLINK_OK);
    ASSERT_EQ(p.state, MAVLINK_PARSE_IDLE);
}

TEST(test_parser_init_null)
{
    ASSERT_EQ(mavlink_parser_init(NULL), MAVLINK_ERR_PARAM);
}

TEST(test_crc16_empty)
{
    uint16_t crc = mavlink_crc16(NULL, 0, 0xFFFF);
    ASSERT_EQ_HEX(crc, 0xFFFF);
}

TEST(test_crc16_known)
{
    const uint8_t data[] = "123456789";
    uint16_t crc = mavlink_crc16(data, 9, 0xFFFF);
    ASSERT_EQ_HEX(crc, 0x6F91);
}

TEST(test_crc16_incremental)
{
    const uint8_t data[] = "123456789";
    uint16_t crc_one = mavlink_crc16(data, 9, 0xFFFF);
    uint16_t crc_inc = mavlink_crc16(data, 4, 0xFFFF);
    crc_inc = mavlink_crc16(&data[4], 5, crc_inc);
    ASSERT_EQ_HEX(crc_one, crc_inc);
}

TEST(test_crc_extra_known)
{
    ASSERT_EQ(mavlink_msg_crc_extra(0),  50);
    ASSERT_EQ(mavlink_msg_crc_extra(1),  124);
    ASSERT_EQ(mavlink_msg_crc_extra(24), 24);
    ASSERT_EQ(mavlink_msg_crc_extra(30), 39);
    ASSERT_EQ(mavlink_msg_crc_extra(33), 104);
}

TEST(test_crc_extra_unknown)
{
    ASSERT_EQ(mavlink_msg_crc_extra(999), 0);
    ASSERT_EQ(mavlink_msg_crc_extra(0xFFFFFF), 0);
}

TEST(test_serialize_heartbeat)
{
    uint8_t buf[64];
    uint16_t len = build_heartbeat_frame(buf, sizeof(buf), 0, 1, 1);
    ASSERT_EQ(len, 21);
    ASSERT_EQ(buf[0], MAVLINK_STX);
    ASSERT_EQ(buf[1], 9);
    ASSERT_EQ(buf[5], 1);
    ASSERT_EQ(buf[6], 1);
}

TEST(test_serialize_null_param)
{
    mavlink_msg_t msg;
    uint8_t buf[64];
    uint16_t out_len;
    memset(&msg, 0, sizeof(msg));
    ASSERT_EQ(mavlink_serialize(NULL, buf, 64, &out_len, 50), MAVLINK_ERR_PARAM);
    ASSERT_EQ(mavlink_serialize(&msg, NULL, 64, &out_len, 50), MAVLINK_ERR_PARAM);
    ASSERT_EQ(mavlink_serialize(&msg, buf, 64, NULL, 50), MAVLINK_ERR_PARAM);
}

TEST(test_serialize_buffer_too_small)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len = 9;
    uint8_t buf[10];
    uint16_t out_len;
    ASSERT_EQ(mavlink_serialize(&msg, buf, sizeof(buf), &out_len, 50), MAVLINK_ERR_SIZE);
}

TEST(test_serialize_zero_payload)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.sysid  = 1;
    msg.compid = 1;
    uint8_t buf[64];
    uint16_t out_len;
    ASSERT_EQ(mavlink_serialize(&msg, buf, sizeof(buf), &out_len, 50), MAVLINK_OK);
    ASSERT_EQ(out_len, 12);
}

TEST(test_parse_heartbeat_roundtrip)
{
    uint8_t frame[64];
    uint16_t flen = build_heartbeat_frame(frame, sizeof(frame), 42, 1, 200);
    ASSERT_TRUE(flen > 0);
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    mavlink_parse_state_t st = parse_frame(&p, frame, flen);
    ASSERT_EQ(st, MAVLINK_PARSE_COMPLETE);
    const mavlink_msg_t *msg = mavlink_get_msg(&p);
    ASSERT_TRUE(msg != NULL);
    ASSERT_EQ(msg->stx, MAVLINK_STX);
    ASSERT_EQ(msg->len, 9);
    ASSERT_EQ(msg->seq, 42);
    ASSERT_EQ(msg->sysid, 1);
    ASSERT_EQ(msg->compid, 200);
    ASSERT_EQ(msg->msgid, MAVLINK_MSG_ID_HEARTBEAT);
    ASSERT_EQ(msg->payload[4], 6);
    ASSERT_EQ(msg->payload[8], 3);
}

TEST(test_parse_with_garbage_prefix)
{
    uint8_t frame[80];
    frame[0] = 0xAA; frame[1] = 0xBB; frame[2] = 0xCC;
    uint16_t flen = build_heartbeat_frame(&frame[3], sizeof(frame) - 3, 0, 1, 1);
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    mavlink_parse_state_t st = parse_frame(&p, frame, (uint16_t)(3u + flen));
    ASSERT_EQ(st, MAVLINK_PARSE_COMPLETE);
    ASSERT_EQ(mavlink_get_msg(&p)->msgid, MAVLINK_MSG_ID_HEARTBEAT);
}

TEST(test_parse_bad_crc)
{
    uint8_t frame[64];
    uint16_t flen = build_heartbeat_frame(frame, sizeof(frame), 0, 1, 1);
    frame[flen - 1] ^= 0xFF;
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    ASSERT_EQ(parse_frame(&p, frame, flen), MAVLINK_PARSE_ERROR);
    ASSERT_TRUE(mavlink_get_msg(&p) == NULL);
}

TEST(test_parse_truncated_frame)
{
    uint8_t frame[64];
    uint16_t flen = build_heartbeat_frame(frame, sizeof(frame), 0, 1, 1);
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    ASSERT_EQ(parse_frame(&p, frame, (uint16_t)(flen / 2)), MAVLINK_PARSE_IN_PROGRESS);
    ASSERT_TRUE(mavlink_get_msg(&p) == NULL);
}

TEST(test_parse_sequence_wrap)
{
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    uint8_t frame[64];
    uint16_t flen;
    flen = build_heartbeat_frame(frame, sizeof(frame), 255, 1, 1);
    ASSERT_EQ(parse_frame(&p, frame, flen), MAVLINK_PARSE_COMPLETE);
    ASSERT_EQ(mavlink_get_msg(&p)->seq, 255);
    flen = build_heartbeat_frame(frame, sizeof(frame), 0, 1, 1);
    ASSERT_EQ(parse_frame(&p, frame, flen), MAVLINK_PARSE_COMPLETE);
    ASSERT_EQ(mavlink_get_msg(&p)->seq, 0);
}

TEST(test_parse_auto_reset_after_complete)
{
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    uint8_t frame[64];
    uint16_t flen = build_heartbeat_frame(frame, sizeof(frame), 1, 1, 1);
    ASSERT_EQ(parse_frame(&p, frame, flen), MAVLINK_PARSE_COMPLETE);
    flen = build_heartbeat_frame(frame, sizeof(frame), 2, 1, 1);
    ASSERT_EQ(parse_frame(&p, frame, flen), MAVLINK_PARSE_COMPLETE);
    ASSERT_EQ(mavlink_get_msg(&p)->seq, 2);
}

TEST(test_parse_auto_reset_after_error)
{
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    uint8_t frame[64];
    uint16_t flen = build_heartbeat_frame(frame, sizeof(frame), 1, 1, 1);
    frame[flen - 1] ^= 0xFF;
    ASSERT_EQ(parse_frame(&p, frame, flen), MAVLINK_PARSE_ERROR);
    flen = build_heartbeat_frame(frame, sizeof(frame), 2, 1, 1);
    ASSERT_EQ(parse_frame(&p, frame, flen), MAVLINK_PARSE_COMPLETE);
    ASSERT_EQ(mavlink_get_msg(&p)->seq, 2);
}

TEST(test_roundtrip_attitude)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len = 28; msg.sysid = 1; msg.compid = 1; msg.seq = 10;
    msg.msgid = MAVLINK_MSG_ID_ATTITUDE;
    for (uint8_t i = 0; i < 28; i++) msg.payload[i] = i;
    uint8_t buf[64]; uint16_t out_len;
    ASSERT_EQ(mavlink_serialize(&msg, buf, sizeof(buf), &out_len, 39), MAVLINK_OK);
    ASSERT_EQ(out_len, 10 + 28 + 2);
    mavlink_parser_t p; mavlink_parser_init(&p);
    ASSERT_EQ(parse_frame(&p, buf, out_len), MAVLINK_PARSE_COMPLETE);
    const mavlink_msg_t *parsed = mavlink_get_msg(&p);
    ASSERT_TRUE(parsed != NULL);
    ASSERT_EQ(parsed->msgid, MAVLINK_MSG_ID_ATTITUDE);
    ASSERT_EQ(parsed->len, 28);
    ASSERT_EQ(memcmp(parsed->payload, msg.payload, 28), 0);
}

TEST(test_roundtrip_gps_raw_int)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len = 30; msg.sysid = 2; msg.compid = 1; msg.seq = 100;
    msg.msgid = MAVLINK_MSG_ID_GPS_RAW_INT;
    for (uint8_t i = 0; i < 30; i++) msg.payload[i] = (uint8_t)(i * 7);
    uint8_t buf[64]; uint16_t out_len;
    ASSERT_EQ(mavlink_serialize(&msg, buf, sizeof(buf), &out_len, 24), MAVLINK_OK);
    mavlink_parser_t p; mavlink_parser_init(&p);
    ASSERT_EQ(parse_frame(&p, buf, out_len), MAVLINK_PARSE_COMPLETE);
    const mavlink_msg_t *parsed = mavlink_get_msg(&p);
    ASSERT_TRUE(parsed != NULL);
    ASSERT_EQ(parsed->msgid, MAVLINK_MSG_ID_GPS_RAW_INT);
    ASSERT_EQ(parsed->sysid, 2);
    ASSERT_EQ(memcmp(parsed->payload, msg.payload, 30), 0);
}

TEST(test_roundtrip_global_position_int)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len = 28; msg.sysid = 1; msg.compid = 1; msg.seq = 200;
    msg.msgid = MAVLINK_MSG_ID_GLOBAL_POSITION_INT;
    for (uint8_t i = 0; i < 28; i++) msg.payload[i] = (uint8_t)(0xFF - i);
    uint8_t buf[64]; uint16_t out_len;
    ASSERT_EQ(mavlink_serialize(&msg, buf, sizeof(buf), &out_len, 104), MAVLINK_OK);
    mavlink_parser_t p; mavlink_parser_init(&p);
    ASSERT_EQ(parse_frame(&p, buf, out_len), MAVLINK_PARSE_COMPLETE);
    const mavlink_msg_t *parsed = mavlink_get_msg(&p);
    ASSERT_TRUE(parsed != NULL);
    ASSERT_EQ(parsed->msgid, MAVLINK_MSG_ID_GLOBAL_POSITION_INT);
    ASSERT_EQ(parsed->seq, 200);
}

TEST(test_roundtrip_sys_status)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len = 31; msg.sysid = 1; msg.compid = 1; msg.seq = 55;
    msg.msgid = MAVLINK_MSG_ID_SYS_STATUS;
    for (uint8_t i = 0; i < 31; i++) msg.payload[i] = (uint8_t)(i + 0x10);
    uint8_t buf[64]; uint16_t out_len;
    ASSERT_EQ(mavlink_serialize(&msg, buf, sizeof(buf), &out_len, 124), MAVLINK_OK);
    mavlink_parser_t p; mavlink_parser_init(&p);
    ASSERT_EQ(parse_frame(&p, buf, out_len), MAVLINK_PARSE_COMPLETE);
    const mavlink_msg_t *parsed = mavlink_get_msg(&p);
    ASSERT_TRUE(parsed != NULL);
    ASSERT_EQ(parsed->msgid, MAVLINK_MSG_ID_SYS_STATUS);
    ASSERT_EQ(parsed->len, 31);
}

TEST(test_serialize_24bit_msgid)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len = 4; msg.sysid = 1; msg.compid = 1;
    msg.msgid = 0xABCDEF;
    msg.payload[0] = 0x11; msg.payload[1] = 0x22;
    msg.payload[2] = 0x33; msg.payload[3] = 0x44;
    uint8_t buf[32]; uint16_t out_len;
    ASSERT_EQ(mavlink_serialize(&msg, buf, sizeof(buf), &out_len, 0), MAVLINK_OK);
    ASSERT_EQ(buf[7], 0xEF);
    ASSERT_EQ(buf[8], 0xCD);
    ASSERT_EQ(buf[9], 0xAB);
    mavlink_parser_t p; mavlink_parser_init(&p);
    ASSERT_EQ(parse_frame(&p, buf, out_len), MAVLINK_PARSE_COMPLETE);
    ASSERT_EQ(mavlink_get_msg(&p)->msgid, (uint32_t)0xABCDEF);
}

TEST(test_roundtrip_max_payload)
{
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len = 255; msg.sysid = 1; msg.compid = 1; msg.seq = 77; msg.msgid = 42;
    for (int i = 0; i < 255; i++) msg.payload[i] = (uint8_t)i;
    uint8_t buf[MAVLINK_FRAME_MAX]; uint16_t out_len;
    ASSERT_EQ(mavlink_serialize(&msg, buf, sizeof(buf), &out_len, 0), MAVLINK_OK);
    ASSERT_EQ(out_len, MAVLINK_FRAME_MAX);
    mavlink_parser_t p; mavlink_parser_init(&p);
    ASSERT_EQ(parse_frame(&p, buf, out_len), MAVLINK_PARSE_COMPLETE);
    const mavlink_msg_t *parsed = mavlink_get_msg(&p);
    ASSERT_TRUE(parsed != NULL);
    ASSERT_EQ(parsed->len, 255);
    ASSERT_EQ(memcmp(parsed->payload, msg.payload, 255), 0);
}

TEST(test_get_msg_null_parser) { ASSERT_TRUE(mavlink_get_msg(NULL) == NULL); }
TEST(test_get_msg_idle) { mavlink_parser_t p; mavlink_parser_init(&p); ASSERT_TRUE(mavlink_get_msg(&p) == NULL); }
TEST(test_parse_byte_null) { ASSERT_EQ(mavlink_parse_byte(NULL, 0xFD), MAVLINK_PARSE_ERROR); }

TEST(test_error_codes)
{
    ASSERT_EQ((int)MAVLINK_OK,        0);
    ASSERT_EQ((int)MAVLINK_ERR_PARAM, -1);
    ASSERT_EQ((int)MAVLINK_ERR_SIZE,  -2);
    ASSERT_EQ((int)MAVLINK_ERR_CRC,   -3);
}

TEST(test_parse_adversarial)
{
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    for (int i = 0; i < 64; i++) mavlink_parse_byte(&p, 0x00);
    mavlink_parser_init(&p);
    for (int i = 0; i < 64; i++) mavlink_parse_byte(&p, 0xFF);
    mavlink_parser_init(&p);
    mavlink_parse_byte(&p, MAVLINK_STX);
    for (int i = 0; i < 300; i++) mavlink_parse_byte(&p, 0xAA);
}

TEST(test_parse_mini_fuzz)
{
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    unsigned seed = 42;
    for (int i = 0; i < 2000; i++) {
        seed = seed * 1103515245u + 12345u;
        mavlink_parse_byte(&p, (uint8_t)(seed >> 16));
    }
}

TEST(test_parse_back_to_back_frames)
{
    uint8_t stream[128];
    uint16_t len1 = build_heartbeat_frame(stream, sizeof(stream), 10, 1, 1);
    uint16_t len2 = build_heartbeat_frame(&stream[len1],
                        (uint16_t)(sizeof(stream) - len1), 11, 1, 1);
    mavlink_parser_t p;
    mavlink_parser_init(&p);
    int complete_count = 0;
    uint8_t last_seq = 0;
    for (uint16_t i = 0; i < (uint16_t)(len1 + len2); i++) {
        mavlink_parse_state_t st = mavlink_parse_byte(&p, stream[i]);
        if (st == MAVLINK_PARSE_COMPLETE) {
            last_seq = mavlink_get_msg(&p)->seq;
            complete_count++;
        }
    }
    ASSERT_EQ(complete_count, 2);
    ASSERT_EQ(last_seq, 11);
}

TEST_MAIN("mavlink")
    RUN(test_parser_init);
    RUN(test_parser_init_null);
    RUN(test_crc16_empty);
    RUN(test_crc16_known);
    RUN(test_crc16_incremental);
    RUN(test_crc_extra_known);
    RUN(test_crc_extra_unknown);
    RUN(test_serialize_heartbeat);
    RUN(test_serialize_null_param);
    RUN(test_serialize_buffer_too_small);
    RUN(test_serialize_zero_payload);
    RUN(test_parse_heartbeat_roundtrip);
    RUN(test_parse_with_garbage_prefix);
    RUN(test_parse_bad_crc);
    RUN(test_parse_truncated_frame);
    RUN(test_parse_sequence_wrap);
    RUN(test_parse_auto_reset_after_complete);
    RUN(test_parse_auto_reset_after_error);
    RUN(test_roundtrip_attitude);
    RUN(test_roundtrip_gps_raw_int);
    RUN(test_roundtrip_global_position_int);
    RUN(test_roundtrip_sys_status);
    RUN(test_serialize_24bit_msgid);
    RUN(test_roundtrip_max_payload);
    RUN(test_get_msg_null_parser);
    RUN(test_get_msg_idle);
    RUN(test_parse_byte_null);
    RUN(test_error_codes);
    RUN(test_parse_adversarial);
    RUN(test_parse_mini_fuzz);
    RUN(test_parse_back_to_back_frames);
TEST_MAIN_END()