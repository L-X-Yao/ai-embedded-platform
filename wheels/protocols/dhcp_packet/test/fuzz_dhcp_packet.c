// SPDX-License-Identifier: MIT
// Fuzz harness for dhcp_packet — parse random byte sequences
#include "dhcp_packet.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    srand(42);

    // Random byte streams — parse header
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 601;
        uint8_t buf[601];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        dhcp_header_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        dhcp_packet_parse(buf, (size_t)len, &hdr);
    }

    // Random byte streams — option iteration
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 601;
        uint8_t buf[601];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        size_t offset = (size_t)(rand() % (len + 1));
        dhcp_option_t opt;
        for (int k = 0; k < 100; k++) {
            int rc = dhcp_packet_next_option(buf, (size_t)len, &offset, &opt);
            if (rc <= 0) break;
        }
    }

    // Random byte streams — get_option
    for (int i = 0; i < 5000; i++) {
        int len = rand() % 601;
        uint8_t buf[601];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        dhcp_option_t opt;
        dhcp_packet_get_option(buf, (size_t)len, (uint8_t)(rand() & 0xFF), &opt);
    }

    // Boundary: empty
    {
        dhcp_header_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        dhcp_packet_parse(NULL, 0, &hdr);
    }

    // Boundary: minimal length
    {
        uint8_t buf[240];
        memset(buf, 0, sizeof(buf));
        dhcp_header_t hdr;
        dhcp_packet_parse(buf, sizeof(buf), &hdr);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[576];
        memset(buf, 0xFF, sizeof(buf));
        dhcp_header_t hdr;
        dhcp_packet_parse(buf, sizeof(buf), &hdr);
    }

    // Boundary: valid DISCOVER then re-parse
    {
        uint8_t buf[576];
        uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
        int len = dhcp_packet_build_discover(buf, sizeof(buf), 0x12345678, mac);
        if (len > 0) {
            dhcp_header_t hdr;
            dhcp_packet_parse(buf, (size_t)len, &hdr);
        }
    }

    // option_u32 / option_ip edge cases
    {
        dhcp_packet_option_u32(NULL);
        uint8_t ip[4];
        dhcp_packet_option_ip(NULL, ip);
        dhcp_packet_option_ip(NULL, NULL);
    }

    printf("OK: dhcp_packet fuzz 25000 random + boundary inputs, no crash\n");
    return 0;
}
