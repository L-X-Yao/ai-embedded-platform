// SPDX-License-Identifier: MIT
// Fuzz harness for lorawan_packet — parse random byte sequences
#include "lorawan_packet.h"
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

        // Parse as data frame
        lorawan_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        lorawan_parse_data(buf, ulen, &frame);

        // Parse as join request
        uint8_t app_eui[8], dev_eui[8], dev_nonce[2];
        lorawan_parse_join_request(buf, ulen, app_eui, dev_eui, dev_nonce);
    }

    // Boundary: empty
    {
        lorawan_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        lorawan_parse_data(NULL, 0, &frame);

        uint8_t app_eui[8], dev_eui[8], dev_nonce[2];
        lorawan_parse_join_request(NULL, 0, app_eui, dev_eui, dev_nonce);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        lorawan_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        lorawan_parse_data(&one, 1, &frame);

        uint8_t app_eui[8], dev_eui[8], dev_nonce[2];
        lorawan_parse_join_request(&one, 1, app_eui, dev_eui, dev_nonce);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        lorawan_frame_t frame;
        lorawan_parse_data(buf, sizeof(buf), &frame);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        lorawan_frame_t frame;
        lorawan_parse_data(buf, sizeof(buf), &frame);
    }

    printf("OK: lorawan_packet fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
