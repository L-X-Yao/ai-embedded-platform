// SPDX-License-Identifier: MIT
// Fuzz harness for canbus — unpack random byte sequences
#include "canbus.h"
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

        canbus_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        canbus_frame_unpack(buf, (uint16_t)(len > 65535 ? 65535 : len), &frame);
    }

    // Boundary: empty
    {
        canbus_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        canbus_frame_unpack(NULL, 0, &frame);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        canbus_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        canbus_frame_unpack(&one, 1, &frame);
    }

    // Boundary: all zeros
    {
        uint8_t buf[13];
        memset(buf, 0, sizeof(buf));
        canbus_frame_t frame;
        canbus_frame_unpack(buf, sizeof(buf), &frame);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[13];
        memset(buf, 0xFF, sizeof(buf));
        canbus_frame_t frame;
        canbus_frame_unpack(buf, sizeof(buf), &frame);
    }

    // Boundary: max pack size
    {
        uint8_t buf[CANBUS_PACK_MAX_SIZE];
        memset(buf, 0xAA, sizeof(buf));
        canbus_frame_t frame;
        canbus_frame_unpack(buf, sizeof(buf), &frame);
    }

    printf("OK: canbus fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
