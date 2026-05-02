// SPDX-License-Identifier: MIT
// Fuzz harness for ncp5623_parser — feed random bytes
#include "ncp5623_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    srand(42);

    ncp5623_parser_t parser;

    // Random byte streams
    for (int i = 0; i < 10000; i++) {
        ncp5623_parser_init(&parser);
        ncp5623_parser_start(&parser);
        int len = rand() % 1025;
        for (int j = 0; j < len; j++) {
            uint8_t byte = (uint8_t)(rand() & 0xFF);
            ncp5623_evt_t evt;
            ncp5623_parser_feed(&parser, byte, &evt);
        }
        ncp5623_parser_stop(&parser);
    }

    // Boundary: all zeros
    {
        ncp5623_parser_init(&parser);
        ncp5623_parser_start(&parser);
        for (int j = 0; j < 256; j++) {
            ncp5623_evt_t evt;
            ncp5623_parser_feed(&parser, 0x00, &evt);
        }
        ncp5623_parser_stop(&parser);
    }

    // Boundary: all 0xFF
    {
        ncp5623_parser_init(&parser);
        ncp5623_parser_start(&parser);
        for (int j = 0; j < 256; j++) {
            ncp5623_evt_t evt;
            ncp5623_parser_feed(&parser, 0xFF, &evt);
        }
        ncp5623_parser_stop(&parser);
    }

    // Boundary: single byte
    {
        ncp5623_parser_init(&parser);
        ncp5623_parser_start(&parser);
        ncp5623_evt_t evt;
        ncp5623_parser_feed(&parser, 0x00, &evt);
        ncp5623_parser_stop(&parser);
    }

    // Boundary: rapid start/stop without feed
    {
        for (int j = 0; j < 256; j++) {
            ncp5623_parser_init(&parser);
            ncp5623_parser_start(&parser);
            ncp5623_parser_stop(&parser);
        }
    }

    printf("OK: ncp5623_parser fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
