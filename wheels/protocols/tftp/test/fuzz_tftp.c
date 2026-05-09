// SPDX-License-Identifier: MIT
// Fuzz harness for tftp — parse random byte sequences
#include "tftp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    srand(42);

    // Random byte streams — parse
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 601;
        uint8_t buf[601];
        for (int j = 0; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        tftp_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        tftp_parse(buf, (size_t)len, &pkt);
    }

    // Random RRQ-shaped packets (opcode=1 then random)
    for (int i = 0; i < 5000; i++) {
        uint8_t buf[256];
        int len = rand() % 255 + 2;
        buf[0] = 0; buf[1] = 1;  // RRQ opcode
        for (int j = 2; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        tftp_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        tftp_parse(buf, (size_t)len, &pkt);
    }

    // Random DATA-shaped packets (opcode=3 then random)
    for (int i = 0; i < 5000; i++) {
        uint8_t buf[520];
        int len = rand() % 518 + 2;
        buf[0] = 0; buf[1] = 3;  // DATA opcode
        for (int j = 2; j < len; j++) buf[j] = (uint8_t)(rand() & 0xFF);

        tftp_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        tftp_parse(buf, (size_t)len, &pkt);

        // Exercise is_last_data
        tftp_is_last_data(&pkt);
    }

    // Boundary: empty
    {
        tftp_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        tftp_parse(NULL, 0, &pkt);
    }

    // Boundary: single byte
    {
        uint8_t one = 0x00;
        tftp_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        tftp_parse(&one, 1, &pkt);
    }

    // Boundary: all zeros
    {
        uint8_t buf[256];
        memset(buf, 0, sizeof(buf));
        tftp_packet_t pkt;
        tftp_parse(buf, sizeof(buf), &pkt);
    }

    // Boundary: all 0xFF
    {
        uint8_t buf[256];
        memset(buf, 0xFF, sizeof(buf));
        tftp_packet_t pkt;
        tftp_parse(buf, sizeof(buf), &pkt);
    }

    // Boundary: is_last_data on NULL
    {
        tftp_is_last_data(NULL);
    }

    // Valid roundtrip: build then parse all opcodes
    {
        uint8_t buf[600];
        tftp_packet_t pkt;

        // RRQ
        int len = tftp_build_rrq(buf, sizeof(buf), "test.txt", "octet");
        if (len > 0) tftp_parse(buf, (size_t)len, &pkt);

        // WRQ
        len = tftp_build_wrq(buf, sizeof(buf), "out.bin", "octet");
        if (len > 0) tftp_parse(buf, (size_t)len, &pkt);

        // DATA
        uint8_t data[4] = {1, 2, 3, 4};
        len = tftp_build_data(buf, sizeof(buf), 1, data, 4);
        if (len > 0) tftp_parse(buf, (size_t)len, &pkt);

        // ACK
        len = tftp_build_ack(buf, sizeof(buf), 42);
        if (len > 0) tftp_parse(buf, (size_t)len, &pkt);

        // ERROR
        len = tftp_build_error(buf, sizeof(buf), 1, "File not found");
        if (len > 0) tftp_parse(buf, (size_t)len, &pkt);
    }

    printf("OK: tftp fuzz 20000 random + boundary inputs, no crash\n");
    return 0;
}
