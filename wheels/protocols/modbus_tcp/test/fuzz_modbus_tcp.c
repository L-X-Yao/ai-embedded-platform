// SPDX-License-Identifier: MIT
// Fuzz harness for modbus_tcp — parse random byte sequences
#include "modbus_tcp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    srand(42);

    // Random byte streams
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 1025;
        uint8_t buf[1025];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        uint16_t ulen = (uint16_t)(len > 65535 ? 65535 : len);

        modbus_tcp_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        modbus_tcp_parse(buf, ulen, &frame);
    }

    // Boundary: empty
    {
        modbus_tcp_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        modbus_tcp_parse(NULL, 0, &frame);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        modbus_tcp_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        modbus_tcp_parse(&one, 1, &frame);
    }

    // Boundary: exactly MBAP header size (7 bytes)
    {
        uint8_t buf[7] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x01};
        modbus_tcp_frame_t frame;
        modbus_tcp_parse(buf, 7, &frame);
    }

    // Boundary: all zeros
    {
        uint8_t buf[260];
        memset(buf, 0, sizeof(buf));
        modbus_tcp_frame_t frame;
        modbus_tcp_parse(buf, sizeof(buf), &frame);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[260];
        memset(buf, 0xFF, sizeof(buf));
        modbus_tcp_frame_t frame;
        modbus_tcp_parse(buf, sizeof(buf), &frame);
    }

    printf("OK: modbus_tcp fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
