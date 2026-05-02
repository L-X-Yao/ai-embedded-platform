// SPDX-License-Identifier: MIT
// Fuzz harness for coap_packet — parse random byte sequences
#include "coap_packet.h"
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

        coap_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        coap_packet_parse(buf, (size_t)len, &pkt);
    }

    // Boundary: empty
    {
        coap_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        coap_packet_parse(NULL, 0, &pkt);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        coap_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        coap_packet_parse(&one, 1, &pkt);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        coap_packet_t pkt;
        coap_packet_parse(buf, sizeof(buf), &pkt);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        coap_packet_t pkt;
        coap_packet_parse(buf, sizeof(buf), &pkt);
    }

    // Boundary: minimal valid-looking header (version=1, 4 bytes header)
    {
        uint8_t buf[4] = {0x40, 0x01, 0x00, 0x01}; // CON GET
        coap_packet_t pkt;
        coap_packet_parse(buf, 4, &pkt);
    }

    printf("OK: coap_packet fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
