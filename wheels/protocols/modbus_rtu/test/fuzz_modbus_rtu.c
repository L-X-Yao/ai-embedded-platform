// SPDX-License-Identifier: MIT
// Fuzz harness for modbus_rtu — parse random byte sequences
#include "modbus_rtu.h"
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

        modbus_rtu_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        modbus_rtu_parse(buf, ulen, &frame);

        // Also check CRC on random data
        modbus_rtu_check_crc(buf, ulen);
    }

    // Boundary: empty
    {
        modbus_rtu_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        modbus_rtu_parse(NULL, 0, &frame);
        modbus_rtu_check_crc(NULL, 0);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        modbus_rtu_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        modbus_rtu_parse(&one, 1, &frame);
    }

    // Boundary: min frame size (4 bytes)
    {
        uint8_t buf[4] = {0x01, 0x03, 0x00, 0x00};
        modbus_rtu_frame_t frame;
        modbus_rtu_parse(buf, 4, &frame);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        modbus_rtu_frame_t frame;
        modbus_rtu_parse(buf, 256, &frame);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        modbus_rtu_frame_t frame;
        modbus_rtu_parse(buf, 256, &frame);
    }

    printf("OK: modbus_rtu fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
