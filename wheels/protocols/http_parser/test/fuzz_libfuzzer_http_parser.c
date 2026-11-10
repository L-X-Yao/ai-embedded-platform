// SPDX-License-Identifier: MIT
// LibFuzzer harness for http_parser — coverage-guided fuzzing
//
// Random bytes almost never spell Content-Length, so the harness also wraps
// the input in a message whose value field and body are fuzz data.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. fuzz_libfuzzer_http_parser.c ../http_parser.c -o fuzz_http_parser
// Run:     ./fuzz_http_parser -max_total_time=60 corpus_http_parser/

#include "../http_parser.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void check_headers(const http_parser_header_t *headers, int count) {
    const http_parser_header_t *found = NULL;
    http_parser_find_header(headers, count, "host", &found);
    http_parser_find_header(headers, count, "Content-Length", &found);
    (void)http_parser_get_content_length(headers, count);
}

static void parse_both(const char *text, size_t size) {
    // Parse as HTTP request
    http_parser_request_t req;
    memset(&req, 0, sizeof(req));
    if (http_parser_parse_request(text, size, &req) == HTTP_PARSER_OK) {
        if (req.total_consumed > size) __builtin_trap();
        check_headers(req.headers, req.header_count);
    }

    // Parse as HTTP response
    http_parser_response_t resp;
    memset(&resp, 0, sizeof(resp));
    if (http_parser_parse_response(text, size, &resp) == HTTP_PARSER_OK) {
        if (resp.total_consumed > size) __builtin_trap();
        check_headers(resp.headers, resp.header_count);
    }

    // Parse as the reply to a HEAD request, which never has a body
    memset(&resp, 0, sizeof(resp));
    if (http_parser_parse_response_to_head(text, size, &resp) == HTTP_PARSER_OK) {
        if (resp.total_consumed > size || resp.body_len != 0) __builtin_trap();
        check_headers(resp.headers, resp.header_count);
    }
}

// Exact-size message [start line][value][blank line][body], so ASan sees any
// read past the end. The first input byte says how much of the rest is value.
static void parse_wrapped(const char *start, const uint8_t *data, size_t size) {
    static const char blank[] = "\r\n\r\n";
    size_t start_len = strlen(start);
    size_t blank_len = strlen(blank);

    if (size == 0) return;
    size_t value_len = data[0] % 24;
    if (value_len > size - 1) value_len = size - 1;
    size_t body_len = size - 1 - value_len;

    size_t total = start_len + value_len + blank_len + body_len;
    char *msg = malloc(total);
    if (!msg) return;

    char *p = msg;
    memcpy(p, start, start_len);
    p += start_len;
    memcpy(p, data + 1, value_len);
    p += value_len;
    memcpy(p, blank, blank_len);
    p += blank_len;
    memcpy(p, data + 1 + value_len, body_len);

    parse_both(msg, total);
    free(msg);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // http_parser takes const char*, cast from uint8_t*
    parse_both((const char *)data, size);

    parse_wrapped("POST /a HTTP/1.1\r\nContent-Length: ", data, size);
    parse_wrapped("HTTP/1.1 200 OK\r\nContent-Length: ", data, size);

    return 0;
}
