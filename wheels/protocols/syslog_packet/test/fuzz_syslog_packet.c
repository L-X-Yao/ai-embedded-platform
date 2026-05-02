// SPDX-License-Identifier: MIT
// Fuzz harness for syslog_packet — parse PRI from random strings
#include "syslog_packet.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    srand(42);

    // Random byte streams (as char)
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 1025;
        char buf[1026];
        for (int j = 0; j < len; j++) {
            buf[j] = (char)(rand() & 0xFF);
        }
        buf[len] = '\0';

        uint8_t pri;
        uint16_t offset;
        syslog_parse_pri(buf, (uint16_t)(len > 65535 ? 65535 : len), &pri, &offset);
    }

    // Boundary: empty
    {
        uint8_t pri;
        uint16_t offset;
        syslog_parse_pri(NULL, 0, &pri, &offset);
        syslog_parse_pri("", 0, &pri, &offset);
    }

    // Boundary: single byte
    {
        uint8_t pri;
        uint16_t offset;
        syslog_parse_pri("<", 1, &pri, &offset);
        syslog_parse_pri(">", 1, &pri, &offset);
        syslog_parse_pri("0", 1, &pri, &offset);
    }

    // Boundary: valid-ish PRI
    {
        uint8_t pri;
        uint16_t offset;
        syslog_parse_pri("<0>", 3, &pri, &offset);
        syslog_parse_pri("<191>", 5, &pri, &offset);
        syslog_parse_pri("<999>", 5, &pri, &offset);
    }

    // Boundary: all zeros
    {
        char buf[256];
        memset(buf, 0, sizeof(buf));
        uint8_t pri;
        uint16_t offset;
        syslog_parse_pri(buf, 256, &pri, &offset);
    }

    // Boundary: all 0xFF
    {
        char buf[256];
        memset(buf, (char)0xFF, sizeof(buf));
        buf[255] = '\0';
        uint8_t pri;
        uint16_t offset;
        syslog_parse_pri(buf, 255, &pri, &offset);
    }

    printf("OK: syslog_packet fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
