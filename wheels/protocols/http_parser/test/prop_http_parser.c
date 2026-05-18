// SPDX-License-Identifier: MIT
/*
 * prop_http_parser.c — property-based tests for http_parser
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_http_parser.c ../http_parser.c -o prop_http_parser
 */

#include "test_harness.h"
#include "property_test.h"
#include "../http_parser.h"
#include <string.h>
#include <stdio.h>

/* Helper: build a valid HTTP request string from random parts */
static int build_request(uint32_t *s, char *buf, size_t cap,
                         const char **exp_method, const char **exp_uri)
{
    static const char *methods[] = {"GET", "POST", "PUT", "DELETE", "HEAD", "PATCH"};
    int mi = (int)(*s % 6u); *s = *s ^ (*s << 13); *s ^= *s >> 17; *s ^= *s << 5;
    *exp_method = methods[mi];

    /* Generate a URI path: /a..z (1..20 chars) */
    static char uri_buf[32];
    uri_buf[0] = '/';
    int uri_len = 1 + (int)(*s % 20u + 1u);
    *s = *s ^ (*s << 13); *s ^= *s >> 17; *s ^= *s << 5;
    for (int i = 1; i < uri_len; i++) {
        uri_buf[i] = (char)('a' + (*s % 26u));
        *s = *s ^ (*s << 13); *s ^= *s >> 17; *s ^= *s << 5;
    }
    uri_buf[uri_len] = '\0';
    *exp_uri = uri_buf;

    int n = snprintf(buf, cap,
                     "%s %s HTTP/1.1\r\nHost: test.local\r\nContent-Length: 0\r\n\r\n",
                     methods[mi], uri_buf);
    return n;
}

/* Property: parse a synthetically valid request -> fields match */
PROPERTY(parse_valid_request, 10000) {
    char req[512];
    const char *exp_method, *exp_uri;
    int len = build_request(&seed, req, sizeof(req), &exp_method, &exp_uri);
    PROP_ASSERT(len > 0 && len < (int)sizeof(req));

    http_parser_request_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    http_parser_err_t err = http_parser_parse_request(req, (size_t)len, &parsed);
    PROP_ASSERT_EQ(err, HTTP_PARSER_OK);

    /* Method must match */
    size_t mlen = strlen(exp_method);
    PROP_ASSERT_EQ(parsed.method_len, mlen);
    PROP_ASSERT_MEM_EQ(parsed.method, exp_method, mlen);

    /* URI must match */
    size_t ulen = strlen(exp_uri);
    PROP_ASSERT_EQ(parsed.uri_len, ulen);
    PROP_ASSERT_MEM_EQ(parsed.uri, exp_uri, ulen);

    /* Version 1.1 */
    PROP_ASSERT_EQ(parsed.version_major, 1);
    PROP_ASSERT_EQ(parsed.version_minor, 1);

    /* Should find Host header */
    PROP_ASSERT(parsed.header_count >= 1);
}

/* Property: parse is deterministic */
PROPERTY(parse_determinism, 5000) {
    char req[512];
    const char *m, *u;
    int len = build_request(&seed, req, sizeof(req), &m, &u);
    PROP_ASSERT(len > 0);

    http_parser_request_t r1, r2;
    memset(&r1, 0, sizeof(r1));
    memset(&r2, 0, sizeof(r2));
    http_parser_err_t e1 = http_parser_parse_request(req, (size_t)len, &r1);
    http_parser_err_t e2 = http_parser_parse_request(req, (size_t)len, &r2);
    PROP_ASSERT_EQ(e1, e2);
    PROP_ASSERT_EQ(r1.method_len, r2.method_len);
    PROP_ASSERT_EQ(r1.uri_len, r2.uri_len);
    PROP_ASSERT_EQ(r1.header_count, r2.header_count);
}

/* Property: Content-Length extraction agrees with header */
PROPERTY(content_length_consistency, 5000) {
    /* Keep body small enough for the buffer; include actual body bytes
       so the parser does not return INCOMPLETE */
    uint16_t cl = (uint16_t)(ARBI_UINT16(seed) % 128u);
    char req[512];
    int hdr_len = snprintf(req, sizeof(req),
                           "POST /data HTTP/1.1\r\nContent-Length: %u\r\n\r\n",
                           (unsigned)cl);
    PROP_ASSERT(hdr_len > 0 && hdr_len + cl < (int)sizeof(req));
    /* Append cl bytes of body (arbitrary) */
    ARBI_FILL(seed, (uint8_t *)(req + hdr_len), cl);
    int total_len = hdr_len + (int)cl;

    http_parser_request_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    http_parser_err_t err = http_parser_parse_request(req, (size_t)total_len, &parsed);
    PROP_ASSERT_EQ(err, HTTP_PARSER_OK);

    int got_cl = http_parser_get_content_length(parsed.headers, parsed.header_count);
    PROP_ASSERT_EQ(got_cl, (int)cl);

    /* Body length must match Content-Length */
    PROP_ASSERT_EQ(parsed.body_len, cl);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    char garbage[512];
    size_t len = ARBI_SIZE(seed, 512);
    ARBI_FILL(seed, (uint8_t *)garbage, len);

    http_parser_request_t req;
    memset(&req, 0, sizeof(req));
    http_parser_parse_request(garbage, len, &req);

    http_parser_response_t resp;
    memset(&resp, 0, sizeof(resp));
    http_parser_parse_response(garbage, len, &resp);
}

TEST_MAIN("http_parser properties") {
    RUN(parse_valid_request);
    RUN(parse_determinism);
    RUN(content_length_consistency);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
