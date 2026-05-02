// SPDX-License-Identifier: MIT
// Fuzz harness for dns_packet — parse random byte sequences
#include "dns_packet.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    srand(42);

    // Random byte streams — parse header + records
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 1025;
        uint8_t buf[1025];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        uint16_t ulen = (uint16_t)(len > 65535 ? 65535 : len);

        // Parse header
        dns_header_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        dns_err_t err = dns_parse_header(buf, ulen, &hdr);

        // If header parses, try records
        if (err == DNS_OK && ulen >= DNS_HEADER_SIZE) {
            dns_record_t rec;
            memset(&rec, 0, sizeof(rec));
            // Try parsing a question record
            dns_parse_record(buf, ulen, DNS_HEADER_SIZE, &rec, false);
            // Try parsing an answer record
            dns_parse_record(buf, ulen, DNS_HEADER_SIZE, &rec, true);
        }

        // Decode name at various offsets
        char name[DNS_NAME_MAX];
        for (uint16_t off = 0; off < ulen && off < 32; off++) {
            dns_decode_name(buf, ulen, off, name, sizeof(name));
        }
    }

    // Boundary: empty
    {
        dns_header_t hdr;
        dns_parse_header(NULL, 0, &hdr);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        dns_header_t hdr;
        dns_parse_header(&one, 1, &hdr);

        char name[DNS_NAME_MAX];
        dns_decode_name(&one, 1, 0, name, sizeof(name));
    }

    // Boundary: all zeros (12 bytes = min header)
    {
        uint8_t buf[12];
        memset(buf, 0, sizeof(buf));
        dns_header_t hdr;
        dns_parse_header(buf, sizeof(buf), &hdr);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        dns_header_t hdr;
        dns_parse_header(buf, sizeof(buf), &hdr);

        dns_record_t rec;
        dns_parse_record(buf, sizeof(buf), DNS_HEADER_SIZE, &rec, true);

        char name[DNS_NAME_MAX];
        dns_decode_name(buf, sizeof(buf), 0, name, sizeof(name));
    }

    // Pointer loops (self-referencing DNS compression pointer)
    {
        uint8_t buf[14] = {0};
        buf[12] = 0xC0; // pointer to offset 12 (self-reference)
        buf[13] = 12;
        char name[DNS_NAME_MAX];
        dns_decode_name(buf, sizeof(buf), 12, name, sizeof(name));
    }

    printf("OK: dns_packet fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
