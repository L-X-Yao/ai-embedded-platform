/*
 * prop_mavlink.c -- property-based tests for mavlink
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness
 *        prop_mavlink.c ../mavlink.c -o prop_mavlink
 */

#include "test_harness.h"
#include "property_test.h"
#include "../mavlink.h"
#include <string.h>

/* --- Property 1: serialize/parse roundtrip preserves all fields --- */

/* Known msgid/crc_extra pairs for roundtrip testing */
static const uint32_t known_msgids[] = { 0, 1, 24, 30, 33 };
static const uint8_t  known_crc_extras[] = { 50, 124, 24, 39, 104 };

PROPERTY(serialize_parse_roundtrip, 10000) {
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.len    = (uint8_t)(ARBI_UINT8(seed) % 128 + 1);  /* 1..128 */
    msg.seq    = ARBI_UINT8(seed);
    msg.sysid  = ARBI_UINT8(seed);
    msg.compid = ARBI_UINT8(seed);

    /* Use a known msgid so the parser's CRC_EXTRA lookup matches */
    uint8_t idx = ARBI_UINT8(seed) % 5;
    msg.msgid = known_msgids[idx];
    uint8_t crc_extra = known_crc_extras[idx];

    ARBI_FILL(seed, msg.payload, msg.len);

    uint8_t buf[MAVLINK_FRAME_MAX];
    uint16_t out_len = 0;
    mavlink_err_t rc = mavlink_serialize(&msg, buf, sizeof(buf),
                                         &out_len, crc_extra);
    PROP_ASSERT_EQ(rc, MAVLINK_OK);
    PROP_ASSERT(out_len > 0);

    /* parse it back */
    mavlink_parser_t parser;
    mavlink_parser_init(&parser);
    mavlink_parse_state_t state = MAVLINK_PARSE_IDLE;
    uint16_t i;
    for (i = 0; i < out_len; i++) {
        state = mavlink_parse_byte(&parser, buf[i]);
    }
    PROP_ASSERT_EQ(state, MAVLINK_PARSE_COMPLETE);

    const mavlink_msg_t *parsed = mavlink_get_msg(&parser);
    PROP_ASSERT(parsed != NULL);
    PROP_ASSERT_EQ(parsed->len, msg.len);
    PROP_ASSERT_EQ(parsed->seq, msg.seq);
    PROP_ASSERT_EQ(parsed->sysid, msg.sysid);
    PROP_ASSERT_EQ(parsed->compid, msg.compid);
    PROP_ASSERT_EQ(parsed->msgid, msg.msgid);
    PROP_ASSERT_MEM_EQ(parsed->payload, msg.payload, msg.len);
}

/* --- Property 2: random bytes never crash the parser --- */

PROPERTY(random_bytes_no_crash, 10000) {
    mavlink_parser_t parser;
    mavlink_parser_init(&parser);

    int n = (int)ARBI_SIZE(seed, 299) + 1;
    int i;
    for (i = 0; i < n; i++) {
        uint8_t byte = ARBI_UINT8(seed);
        mavlink_parse_byte(&parser, byte);
    }
    /* reaching here without crash is success */
    PROP_ASSERT(1);
}

/* --- Property 3: CRC-16 is incremental --- */

PROPERTY(crc16_incremental, 10000) {
    uint8_t data[128];
    size_t len = ARBI_SIZE(seed, 127) + 1;
    ARBI_FILL(seed, data, len);

    /* split at random point */
    size_t split = ARBI_SIZE(seed, len - 1);

    uint16_t crc_full = mavlink_crc16(data, (uint16_t)len, 0xFFFF);
    uint16_t crc_inc  = mavlink_crc16(data, (uint16_t)split, 0xFFFF);
    crc_inc = mavlink_crc16(data + split, (uint16_t)(len - split), crc_inc);

    PROP_ASSERT_EQ(crc_full, crc_inc);
}

/* --- Property 4: corrupted CRC causes parse error --- */

PROPERTY(corrupted_crc_detected, 5000) {
    mavlink_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.len    = (uint8_t)(ARBI_UINT8(seed) % 32 + 1);
    msg.seq    = ARBI_UINT8(seed);
    msg.sysid  = 1;
    msg.compid = 1;
    uint8_t idx = ARBI_UINT8(seed) % 5;
    msg.msgid  = known_msgids[idx];
    ARBI_FILL(seed, msg.payload, msg.len);

    uint8_t buf[MAVLINK_FRAME_MAX];
    uint16_t out_len = 0;
    mavlink_serialize(&msg, buf, sizeof(buf), &out_len, known_crc_extras[idx]);

    /* flip a bit in the checksum */
    uint8_t bit = ARBI_UINT8(seed) % 16;
    if (bit < 8) {
        buf[out_len - 2] ^= (uint8_t)(1u << bit);
    } else {
        buf[out_len - 1] ^= (uint8_t)(1u << (bit - 8));
    }

    mavlink_parser_t parser;
    mavlink_parser_init(&parser);
    mavlink_parse_state_t state = MAVLINK_PARSE_IDLE;
    uint16_t i;
    for (i = 0; i < out_len; i++) {
        state = mavlink_parse_byte(&parser, buf[i]);
    }
    PROP_ASSERT_EQ(state, MAVLINK_PARSE_ERROR);
}

/* --- Property 5: back-to-back frames parse independently --- */

PROPERTY(back_to_back_frames, 5000) {
    mavlink_msg_t msg1, msg2;
    memset(&msg1, 0, sizeof(msg1));
    memset(&msg2, 0, sizeof(msg2));

    uint8_t idx1 = ARBI_UINT8(seed) % 5;
    uint8_t idx2 = ARBI_UINT8(seed) % 5;

    msg1.len = (uint8_t)(ARBI_UINT8(seed) % 32 + 1);
    msg1.seq = ARBI_UINT8(seed);
    msg1.sysid = 1; msg1.compid = 1;
    msg1.msgid = known_msgids[idx1];
    ARBI_FILL(seed, msg1.payload, msg1.len);

    msg2.len = (uint8_t)(ARBI_UINT8(seed) % 32 + 1);
    msg2.seq = ARBI_UINT8(seed);
    msg2.sysid = 2; msg2.compid = 2;
    msg2.msgid = known_msgids[idx2];
    ARBI_FILL(seed, msg2.payload, msg2.len);

    uint8_t stream[MAVLINK_FRAME_MAX * 2];
    uint16_t len1 = 0, len2 = 0;
    mavlink_serialize(&msg1, stream, sizeof(stream), &len1,
                      known_crc_extras[idx1]);
    mavlink_serialize(&msg2, stream + len1,
                      (uint16_t)(sizeof(stream) - len1), &len2,
                      known_crc_extras[idx2]);

    mavlink_parser_t parser;
    mavlink_parser_init(&parser);
    int complete_count = 0;
    uint16_t i;
    for (i = 0; i < (uint16_t)(len1 + len2); i++) {
        mavlink_parse_state_t st = mavlink_parse_byte(&parser, stream[i]);
        if (st == MAVLINK_PARSE_COMPLETE) complete_count++;
    }
    PROP_ASSERT_EQ(complete_count, 2);
}

TEST_MAIN("mavlink properties") {
    RUN(serialize_parse_roundtrip);
    RUN(random_bytes_no_crash);
    RUN(crc16_incremental);
    RUN(corrupted_crc_detected);
    RUN(back_to_back_frames);
}
TEST_MAIN_END()
