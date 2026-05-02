// SPDX-License-Identifier: MIT
// Fuzz harness for websocket_frame — parse random byte sequences
#include "websocket_frame.h"
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

        websocket_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        websocket_frame_parse(buf, (size_t)len, &frame);
    }

    // Boundary: empty
    {
        websocket_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        websocket_frame_parse(NULL, 0, &frame);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x81; // text frame, FIN
        websocket_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        websocket_frame_parse(&one, 1, &frame);
    }

    // Boundary: two bytes (header with length)
    {
        uint8_t buf[2] = {0x81, 0x00}; // FIN + text, length 0
        websocket_frame_t frame;
        websocket_frame_parse(buf, 2, &frame);
    }

    // Boundary: masked frame header
    {
        uint8_t buf[6] = {0x81, 0x80, 0x01, 0x02, 0x03, 0x04}; // masked, length 0
        websocket_frame_t frame;
        websocket_frame_parse(buf, sizeof(buf), &frame);
    }

    // Boundary: 16-bit extended length
    {
        uint8_t buf[4] = {0x81, 126, 0xFF, 0xFF}; // length = 65535
        websocket_frame_t frame;
        websocket_frame_parse(buf, sizeof(buf), &frame);
    }

    // Boundary: 64-bit extended length
    {
        uint8_t buf[10] = {0x81, 127, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
        websocket_frame_t frame;
        websocket_frame_parse(buf, sizeof(buf), &frame);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        websocket_frame_t frame;
        websocket_frame_parse(buf, sizeof(buf), &frame);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        websocket_frame_t frame;
        websocket_frame_parse(buf, sizeof(buf), &frame);
    }

    printf("OK: websocket_frame fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
