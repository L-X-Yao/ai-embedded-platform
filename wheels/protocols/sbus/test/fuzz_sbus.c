// SPDX-License-Identifier: MIT
// Fuzz harness for sbus -- feed random bytes into parser
#include "sbus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    srand(42);
    int iterations = 10000;

    // Random byte streams -- exercise parser with arbitrary data
    for (int i = 0; i < iterations; i++) {
        sbus_parser_t parser;
        sbus_parser_init(&parser);

        int len = rand() % 1025;
        uint8_t buf[1025];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        // Feed byte-at-a-time
        for (int j = 0; j < len; j++) {
            sbus_parse_state_t st = sbus_parse_byte(&parser, buf[j]);
            if (st == SBUS_PARSE_COMPLETE) {
                // Decode the completed frame
                const uint8_t *raw = sbus_parser_get_buf(&parser);
                sbus_frame_t frame;
                memset(&frame, 0, sizeof(frame));
                sbus_decode(raw, &frame);
            }
        }

        // Exercise decode with random 25 bytes
        if (len >= SBUS_FRAME_LEN) {
            sbus_frame_t frame;
            memset(&frame, 0, sizeof(frame));
            sbus_decode(buf, &frame);
        }

        // Exercise encode with random frame data
        {
            sbus_frame_t frame;
            memset(&frame, 0, sizeof(frame));
            for (int j = 0; j < SBUS_NUM_CHANNELS; j++) {
                frame.channels[j] = (uint16_t)(rand() % (SBUS_CHANNEL_MAX + 1));
            }
            frame.ch17 = rand() & 1;
            frame.ch18 = rand() & 1;
            frame.frame_lost = rand() & 1;
            frame.failsafe = rand() & 1;

            uint8_t encoded[SBUS_FRAME_LEN];
            sbus_encode(&frame, encoded);

            // Round-trip: decode what we just encoded
            sbus_frame_t decoded;
            memset(&decoded, 0, sizeof(decoded));
            sbus_decode(encoded, &decoded);
        }

        // Exercise utility functions with random values
        {
            uint16_t ch_val = (uint16_t)(rand() % 4096);
            sbus_channel_to_us(ch_val);

            uint16_t us_val = (uint16_t)(rand() % 4096);
            sbus_us_to_channel(us_val);
        }
    }

    // Boundary: empty parser (no bytes fed)
    {
        sbus_parser_t parser;
        sbus_parser_init(&parser);
        (void)sbus_parser_get_buf(&parser);
    }

    // Boundary: single header byte
    {
        sbus_parser_t parser;
        sbus_parser_init(&parser);
        sbus_parse_byte(&parser, SBUS_HEADER);
    }

    // Boundary: all zeros (25 bytes)
    {
        uint8_t zeros[SBUS_FRAME_LEN];
        memset(zeros, 0, sizeof(zeros));
        sbus_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        sbus_decode(zeros, &frame);
    }

    // Boundary: all 0xFF (25 bytes)
    {
        uint8_t ffs[SBUS_FRAME_LEN];
        memset(ffs, 0xFF, sizeof(ffs));
        sbus_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        sbus_decode(ffs, &frame);
    }

    // Boundary: valid header + footer, random payload
    {
        sbus_parser_t parser;
        sbus_parser_init(&parser);
        uint8_t raw[SBUS_FRAME_LEN];
        raw[0] = SBUS_HEADER;
        for (int j = 1; j < SBUS_FRAME_LEN - 1; j++) raw[j] = (uint8_t)(rand() & 0xFF);
        raw[SBUS_FRAME_LEN - 1] = SBUS_FOOTER;
        for (int j = 0; j < SBUS_FRAME_LEN; j++) {
            sbus_parse_byte(&parser, raw[j]);
        }
    }

    // Boundary: NULL params
    {
        sbus_decode(NULL, NULL);
        sbus_encode(NULL, NULL);
        sbus_parser_init(NULL);
    }

    // Boundary: utility clamping
    {
        sbus_channel_to_us(0);
        sbus_channel_to_us(SBUS_CHANNEL_MAX);
        sbus_channel_to_us(0xFFFF);
        sbus_us_to_channel(0);
        sbus_us_to_channel(SBUS_US_MAX);
        sbus_us_to_channel(0xFFFF);
    }

    printf("OK: sbus fuzz %d random + boundary inputs, no crash\n", iterations);
    return 0;
}
