// SPDX-License-Identifier: MIT
// Fuzz harness for mqtt_packet — parse random byte sequences
#include "mqtt_packet.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    srand(42);

    // Random byte streams — exercise all parse functions
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 1025;
        uint8_t buf[1025];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        size_t ulen = (size_t)len;

        // Get type
        mqtt_packet_get_type(buf, ulen);

        // Parse connack
        mqtt_connack_t connack;
        memset(&connack, 0, sizeof(connack));
        mqtt_packet_parse_connack(buf, ulen, &connack);

        // Parse publish
        mqtt_publish_t pub;
        memset(&pub, 0, sizeof(pub));
        mqtt_packet_parse_publish(buf, ulen, &pub);

        // Parse puback
        uint16_t packet_id;
        mqtt_packet_parse_puback(buf, ulen, &packet_id);

        // Decode remaining length
        int bytes_used = 0;
        mqtt_packet_decode_remaining_length(buf, ulen, &bytes_used);
    }

    // Boundary: empty
    {
        mqtt_packet_get_type(NULL, 0);
        mqtt_connack_t connack;
        mqtt_packet_parse_connack(NULL, 0, &connack);
        mqtt_publish_t pub;
        memset(&pub, 0, sizeof(pub));
        mqtt_packet_parse_publish(NULL, 0, &pub);
        uint16_t pid;
        mqtt_packet_parse_puback(NULL, 0, &pid);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x20; // CONNACK type
        mqtt_packet_get_type(&one, 1);
        mqtt_connack_t connack;
        mqtt_packet_parse_connack(&one, 1, &connack);
    }

    // Boundary: all 0xFF — max remaining length pattern
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        int bytes_used = 0;
        mqtt_packet_decode_remaining_length(buf, sizeof(buf), &bytes_used);
    }

    printf("OK: mqtt_packet fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
