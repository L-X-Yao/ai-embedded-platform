// SPDX-License-Identifier: MIT
// Fuzz harness for at_command — feed random bytes into parser
#include "at_command.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    srand(42);

    at_command_t at;
    at_command_init(&at, NULL); // no HAL needed for feed-only fuzzing

    // Random byte streams
    for (int i = 0; i < 10000; i++) {
        at_command_init(&at, NULL);
        int len = rand() % 1025;
        for (int j = 0; j < len; j++) {
            uint8_t byte = (uint8_t)(rand() & 0xFF);
            at_command_evt_t evt;
            at_command_feed(&at, byte, &evt);
        }
    }

    // Boundary: all zeros
    {
        at_command_init(&at, NULL);
        for (int j = 0; j < 256; j++) {
            at_command_evt_t evt;
            at_command_feed(&at, 0x00, &evt);
        }
    }

    // Boundary: all 0xFF
    {
        at_command_init(&at, NULL);
        for (int j = 0; j < 256; j++) {
            at_command_evt_t evt;
            at_command_feed(&at, 0xFF, &evt);
        }
    }

    // Boundary: single byte
    {
        at_command_init(&at, NULL);
        at_command_evt_t evt;
        at_command_feed(&at, 0x00, &evt);
    }

    // Boundary: rapid CR/LF sequences
    {
        at_command_init(&at, NULL);
        for (int j = 0; j < 512; j++) {
            at_command_evt_t evt;
            at_command_feed(&at, (uint8_t)((j % 2) ? '\r' : '\n'), &evt);
        }
    }

    printf("OK: at_command fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
