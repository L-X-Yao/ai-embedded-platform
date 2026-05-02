// SPDX-License-Identifier: MIT
// Fuzz harness for mqtt5_packet — parse random byte sequences
#include "mqtt5_packet.h"
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

        // Parse connack
        mqtt5_connack_t ack;
        memset(&ack, 0, sizeof(ack));
        mqtt5_parse_connack(buf, ulen, &ack);

        // Parse publish
        mqtt5_publish_t pub;
        memset(&pub, 0, sizeof(pub));
        mqtt5_parse_publish(buf, ulen, &pub);

        // Decode VBI
        uint32_t value;
        uint8_t bytes_used;
        mqtt5_decode_vbi(buf, ulen, &value, &bytes_used);
    }

    // Boundary: empty
    {
        mqtt5_connack_t ack;
        memset(&ack, 0, sizeof(ack));
        mqtt5_parse_connack(NULL, 0, &ack);

        mqtt5_publish_t pub;
        memset(&pub, 0, sizeof(pub));
        mqtt5_parse_publish(NULL, 0, &pub);

        uint32_t value;
        uint8_t bytes_used;
        mqtt5_decode_vbi(NULL, 0, &value, &bytes_used);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x20;
        mqtt5_connack_t ack;
        mqtt5_parse_connack(&one, 1, &ack);

        uint32_t value;
        uint8_t bytes_used;
        mqtt5_decode_vbi(&one, 1, &value, &bytes_used);
    }

    // Boundary: all 0xFF — max VBI encoding
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        uint32_t value;
        uint8_t bytes_used;
        mqtt5_decode_vbi(buf, sizeof(buf), &value, &bytes_used);
    }

    printf("OK: mqtt5_packet fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
