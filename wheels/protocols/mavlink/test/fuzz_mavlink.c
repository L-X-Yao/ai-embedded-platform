// SPDX-License-Identifier: MIT
// Fuzz harness for mavlink -- feed random bytes into parser
#include "mavlink.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    srand(42);
    int iterations = 10000;

    // Random byte streams -- exercise parser with arbitrary data
    for (int i = 0; i < iterations; i++) {
        mavlink_parser_t parser;
        mavlink_parser_init(&parser);

        int len = rand() % 1025;
        uint8_t buf[1025];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        // Feed byte-at-a-time
        for (int j = 0; j < len; j++) {
            mavlink_parse_state_t st = mavlink_parse_byte(&parser, buf[j]);
            if (st == MAVLINK_PARSE_COMPLETE) {
                // Access parsed message
                const mavlink_msg_t *msg = mavlink_get_msg(&parser);
                (void)msg;
            }
        }

        // Also exercise serialize with random msg fields
        mavlink_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.len = (uint8_t)(rand() & 0xFF);
        msg.sysid = (uint8_t)(rand() & 0xFF);
        msg.compid = (uint8_t)(rand() & 0xFF);
        msg.seq = (uint8_t)(rand() & 0xFF);
        msg.msgid = (uint32_t)(rand() & 0xFFFFFF);
        for (int j = 0; j < MAVLINK_MAX_PAYLOAD && j < msg.len; j++) {
            msg.payload[j] = (uint8_t)(rand() & 0xFF);
        }

        uint8_t out_buf[MAVLINK_FRAME_MAX];
        uint16_t out_len = 0;
        uint8_t crc_extra = mavlink_msg_crc_extra(msg.msgid);
        mavlink_serialize(&msg, out_buf, sizeof(out_buf), &out_len, crc_extra);

        // Exercise CRC function with random data
        if (len > 0) {
            mavlink_crc16(buf, (uint16_t)(len > 1024 ? 1024 : len), 0xFFFF);
        }
    }

    // Boundary: empty input (no bytes fed)
    {
        mavlink_parser_t parser;
        mavlink_parser_init(&parser);
        (void)mavlink_get_msg(&parser);
    }

    // Boundary: single STX byte
    {
        mavlink_parser_t parser;
        mavlink_parser_init(&parser);
        mavlink_parse_byte(&parser, MAVLINK_STX);
    }

    // Boundary: all zeros
    {
        mavlink_parser_t parser;
        mavlink_parser_init(&parser);
        uint8_t zeros[MAVLINK_FRAME_MAX];
        memset(zeros, 0, sizeof(zeros));
        for (int j = 0; j < MAVLINK_FRAME_MAX; j++) {
            mavlink_parse_byte(&parser, zeros[j]);
        }
    }

    // Boundary: all 0xFF
    {
        mavlink_parser_t parser;
        mavlink_parser_init(&parser);
        uint8_t ffs[MAVLINK_FRAME_MAX];
        memset(ffs, 0xFF, sizeof(ffs));
        for (int j = 0; j < MAVLINK_FRAME_MAX; j++) {
            mavlink_parse_byte(&parser, ffs[j]);
        }
    }

    // Boundary: serialize with NULL/zero params
    {
        uint16_t out_len = 0;
        mavlink_serialize(NULL, NULL, 0, &out_len, 0);

        mavlink_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        uint8_t buf[1];
        mavlink_serialize(&msg, buf, 0, &out_len, 0);
    }

    // Boundary: CRC with zero length
    {
        mavlink_crc16(NULL, 0, 0xFFFF);
    }

    // Boundary: CRC_EXTRA for unknown message IDs
    {
        mavlink_msg_crc_extra(0xFFFFFF);
        mavlink_msg_crc_extra(999999);
    }

    printf("OK: mavlink fuzz %d random + boundary inputs, no crash\n", iterations);
    return 0;
}
