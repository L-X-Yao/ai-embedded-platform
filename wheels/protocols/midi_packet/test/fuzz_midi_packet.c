// SPDX-License-Identifier: MIT
// Fuzz harness for midi_packet — feed random bytes into streaming parser
#include "midi_packet.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    srand(42);

    midi_parser_t parser;

    // Random byte streams
    for (int i = 0; i < 10000; i++) {
        midi_parser_init(&parser);
        int len = rand() % 1025;
        for (int j = 0; j < len; j++) {
            uint8_t byte = (uint8_t)(rand() & 0xFF);
            midi_msg_t msg;
            midi_parse(&parser, byte, &msg);
        }
    }

    // Boundary: all zeros
    {
        midi_parser_init(&parser);
        for (int j = 0; j < 256; j++) {
            midi_msg_t msg;
            midi_parse(&parser, 0x00, &msg);
        }
    }

    // Boundary: all 0xFF
    {
        midi_parser_init(&parser);
        for (int j = 0; j < 256; j++) {
            midi_msg_t msg;
            midi_parse(&parser, 0xFF, &msg);
        }
    }

    // Boundary: all status bytes
    {
        midi_parser_init(&parser);
        for (int b = 0x80; b <= 0xFF; b++) {
            midi_msg_t msg;
            midi_parse(&parser, (uint8_t)b, &msg);
        }
    }

    // Boundary: SysEx flood
    {
        midi_parser_init(&parser);
        midi_msg_t msg;
        midi_parse(&parser, 0xF0, &msg); // SysEx start
        for (int j = 0; j < 512; j++) {
            midi_parse(&parser, (uint8_t)(j & 0x7F), &msg);
        }
        midi_parse(&parser, 0xF7, &msg); // SysEx end
    }

    printf("OK: midi_packet fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
