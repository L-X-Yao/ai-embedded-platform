/*
 * prop_http_request.c -- property-based tests for http_request
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness
 *        prop_http_request.c ../http_request.c -o prop_http_request
 */

#include "test_harness.h"
#include "property_test.h"
#include "../http_request.h"
#include <string.h>

/* helper: check if needle is found in haystack */
static int contains(const uint8_t *haystack, size_t hlen, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen > hlen) return 0;
    size_t i;
    for (i = 0; i <= hlen - nlen; i++) {
        if (memcmp(haystack + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

/* --- Property 1: output always ends with \r\n\r\n (headers terminator) --- */

PROPERTY(output_ends_with_crlf_crlf, 10000) {
    uint8_t buf[512];
    size_t len = 0;
    http_method_t method = (http_method_t)(ARBI_UINT8(seed) % 4);

    http_request_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.method = method;
    cfg.host   = "host.io";
    cfg.path   = "/test";

    http_request_err_t rc = http_request_build(buf, sizeof(buf), &cfg, &len);
    PROP_ASSERT_EQ(rc, HTTP_REQUEST_OK);
    PROP_ASSERT(len >= 4);
    PROP_ASSERT(buf[len - 4] == '\r');
    PROP_ASSERT(buf[len - 3] == '\n');
    PROP_ASSERT(buf[len - 2] == '\r');
    PROP_ASSERT(buf[len - 1] == '\n');
}

/* --- Property 2: output starts with valid method string --- */

PROPERTY(starts_with_valid_method, 10000) {
    uint8_t buf[512];
    size_t len = 0;
    http_method_t method = (http_method_t)(ARBI_UINT8(seed) % 4);

    http_request_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.method = method;
    cfg.host   = "h.io";
    cfg.path   = "/p";

    http_request_err_t rc = http_request_build(buf, sizeof(buf), &cfg, &len);
    PROP_ASSERT_EQ(rc, HTTP_REQUEST_OK);

    int ok = contains(buf, len, "GET /") ||
             contains(buf, len, "POST /") ||
             contains(buf, len, "PUT /") ||
             contains(buf, len, "HEAD /");
    PROP_ASSERT(ok);
}

/* --- Property 3: output contains HTTP/1.1 --- */

PROPERTY(contains_http_version, 10000) {
    uint8_t buf[512];
    size_t len = 0;
    http_method_t method = (http_method_t)(ARBI_UINT8(seed) % 4);

    http_request_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.method = method;
    cfg.host   = "example.com";
    cfg.path   = "/test";

    http_request_err_t rc = http_request_build(buf, sizeof(buf), &cfg, &len);
    PROP_ASSERT_EQ(rc, HTTP_REQUEST_OK);
    PROP_ASSERT(contains(buf, len, "HTTP/1.1\r\n"));
}

/* --- Property 4: small buffer always returns overflow, never corrupts --- */

PROPERTY(small_buffer_overflow, 10000) {
    size_t cap = ARBI_SIZE(seed, 15);  /* 0..15 bytes */
    uint8_t buf[16];
    size_t len = 0;

    http_request_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.method = HTTP_METHOD_GET;
    cfg.host   = "example.com";
    cfg.path   = "/firmware/v2.bin";

    http_request_err_t rc = http_request_build(buf, cap, &cfg, &len);
    /* smallest possible GET is ~40 bytes, so cap<=15 must overflow */
    PROP_ASSERT_EQ(rc, HTTP_REQUEST_ERR_OVERFLOW);
}

/* --- Property 5: range GET always includes Range header --- */

PROPERTY(range_get_has_range_header, 10000) {
    uint8_t buf[512];
    size_t len = 0;
    size_t offset = (size_t)ARBI_UINT32(seed);
    size_t end = (ARBI_UINT8(seed) & 1) ? (size_t)ARBI_UINT32(seed) : 0;

    http_request_err_t rc = http_request_build_range_get(
        buf, sizeof(buf), "h.io", "/fw", offset, end, &len);
    PROP_ASSERT_EQ(rc, HTTP_REQUEST_OK);
    PROP_ASSERT(contains(buf, len, "Range: bytes="));
}

TEST_MAIN("http_request properties") {
    RUN(output_ends_with_crlf_crlf);
    RUN(starts_with_valid_method);
    RUN(contains_http_version);
    RUN(small_buffer_overflow);
    RUN(range_get_has_range_header);
}
TEST_MAIN_END()
