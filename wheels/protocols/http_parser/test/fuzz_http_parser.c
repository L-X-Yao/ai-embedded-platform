// SPDX-License-Identifier: MIT
// Fuzz harness for http_parser — parse random byte sequences as HTTP
#include "http_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    srand(42);

    // Random byte streams — parse as both request and response
    for (int i = 0; i < 10000; i++) {
        int len = rand() % 1025;
        // +1 for NUL safety
        char buf[1026];
        for (int j = 0; j < len; j++) buf[j] = (char)(rand() & 0xFF);
        buf[len] = '\0';

        http_parser_request_t req;
        memset(&req, 0, sizeof(req));
        http_parser_parse_request(buf, (size_t)len, &req);

        http_parser_response_t resp;
        memset(&resp, 0, sizeof(resp));
        http_parser_parse_response(buf, (size_t)len, &resp);
    }

    // Boundary: empty
    {
        http_parser_request_t req;
        memset(&req, 0, sizeof(req));
        http_parser_parse_request(NULL, 0, &req);

        http_parser_response_t resp;
        memset(&resp, 0, sizeof(resp));
        http_parser_parse_response(NULL, 0, &resp);
    }

    // Boundary: single byte
    {
        char one = 'G';
        http_parser_request_t req;
        memset(&req, 0, sizeof(req));
        http_parser_parse_request(&one, 1, &req);

        http_parser_response_t resp;
        memset(&resp, 0, sizeof(resp));
        http_parser_parse_response(&one, 1, &resp);
    }

    // Boundary: all zeros
    {
        char buf[256];
        memset(buf, 0, sizeof(buf));
        http_parser_request_t req;
        http_parser_parse_request(buf, sizeof(buf), &req);
    }

    // Boundary: all 0xFF
    {
        char buf[256];
        memset(buf, 0xFF, sizeof(buf));
        http_parser_request_t req;
        http_parser_parse_request(buf, sizeof(buf), &req);
    }

    // Boundary: malformed headers with many \r\n
    {
        char buf[512];
        for (int j = 0; j < 512; j += 2) {
            buf[j] = '\r';
            buf[j + 1] = '\n';
        }
        http_parser_request_t req;
        http_parser_parse_request(buf, 512, &req);
    }

    printf("OK: http_parser fuzz 10000 random + boundary inputs, no crash\n");
    return 0;
}
