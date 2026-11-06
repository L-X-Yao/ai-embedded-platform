// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "http_request.h"
#include <string.h>

static int contains(const uint8_t *haystack, size_t hlen,
                    const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen > hlen) return 0;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        if (memcmp(haystack + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

/* ---------- basic GET ---------- */

TEST(test_simple_get)
{
    uint8_t buf[256];
    size_t len;
    http_request_cfg_t cfg = {
        .method = HTTP_METHOD_GET,
        .host   = "example.com",
        .path   = "/firmware/v2.bin",
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len), HTTP_REQUEST_OK);
    ASSERT_TRUE(len > 0);

    const char *expected =
        "GET /firmware/v2.bin HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";
    ASSERT_EQ(len, strlen(expected));
    ASSERT_TRUE(memcmp(buf, expected, len) == 0);
}

/* ---------- POST with body ---------- */

TEST(test_post_with_body)
{
    uint8_t buf[512];
    size_t len;
    const char *body = "{\"version\":\"1.0\"}";
    http_header_t hdrs[] = {
        { .name = "Content-Type", .value = "application/json" },
    };
    http_request_cfg_t cfg = {
        .method       = HTTP_METHOD_POST,
        .host         = "api.example.com",
        .path         = "/report",
        .headers      = hdrs,
        .header_count = 1,
        .body         = (const uint8_t *)body,
        .body_len     = strlen(body),
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len), HTTP_REQUEST_OK);
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(contains(buf, len, "POST /report HTTP/1.1\r\n"));
    ASSERT_TRUE(contains(buf, len, "Content-Type: application/json\r\n"));
    ASSERT_TRUE(contains(buf, len, "Content-Length: 17\r\n"));
    ASSERT_TRUE(contains(buf, len, body));
}

/* ---------- HEAD method ---------- */

TEST(test_head_method)
{
    uint8_t buf[256];
    size_t len;
    http_request_cfg_t cfg = {
        .method = HTTP_METHOD_HEAD,
        .host   = "example.com",
        .path   = "/status",
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len), HTTP_REQUEST_OK);
    ASSERT_TRUE(contains(buf, len, "HEAD /status HTTP/1.1\r\n"));
}

/* ---------- PUT method ---------- */

TEST(test_put_method)
{
    uint8_t buf[256];
    size_t len;
    http_request_cfg_t cfg = {
        .method = HTTP_METHOD_PUT,
        .host   = "example.com",
        .path   = "/data",
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len), HTTP_REQUEST_OK);
    ASSERT_TRUE(contains(buf, len, "PUT /data HTTP/1.1\r\n"));
}

/* ---------- multiple custom headers ---------- */

TEST(test_multiple_headers)
{
    uint8_t buf[512];
    size_t len;
    http_header_t hdrs[] = {
        { .name = "Authorization", .value = "Bearer token123" },
        { .name = "Accept",        .value = "application/octet-stream" },
    };
    http_request_cfg_t cfg = {
        .method       = HTTP_METHOD_GET,
        .host         = "ota.example.com",
        .path         = "/fw",
        .headers      = hdrs,
        .header_count = 2,
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len), HTTP_REQUEST_OK);
    ASSERT_TRUE(contains(buf, len, "Authorization: Bearer token123\r\n"));
    ASSERT_TRUE(contains(buf, len, "Accept: application/octet-stream\r\n"));
}

/* ---------- Range GET (open-ended) ---------- */

TEST(test_range_get_open)
{
    uint8_t buf[256];
    size_t len;
    ASSERT_EQ(http_request_build_range_get(buf, sizeof(buf),
              "ota.example.com", "/fw.bin", 4096, 0, &len),
              HTTP_REQUEST_OK);
    ASSERT_TRUE(contains(buf, len, "GET /fw.bin HTTP/1.1\r\n"));
    ASSERT_TRUE(contains(buf, len, "Host: ota.example.com\r\n"));
    ASSERT_TRUE(contains(buf, len, "Range: bytes=4096-\r\n"));
}

/* ---------- Range GET (bounded) ---------- */

TEST(test_range_get_bounded)
{
    uint8_t buf[256];
    size_t len;
    ASSERT_EQ(http_request_build_range_get(buf, sizeof(buf),
              "host.io", "/fw", 1000, 1999, &len),
              HTTP_REQUEST_OK);
    ASSERT_TRUE(contains(buf, len, "Range: bytes=1000-1999\r\n"));
}

/* ---------- Range GET from offset 0 ---------- */

TEST(test_range_get_from_zero)
{
    uint8_t buf[256];
    size_t len;
    ASSERT_EQ(http_request_build_range_get(buf, sizeof(buf),
              "host.io", "/fw", 0, 0, &len),
              HTTP_REQUEST_OK);
    ASSERT_TRUE(contains(buf, len, "Range: bytes=0-\r\n"));
}

/* ---------- overflow ---------- */

TEST(test_build_overflow)
{
    uint8_t buf[10];
    size_t len;
    http_request_cfg_t cfg = {
        .method = HTTP_METHOD_GET,
        .host   = "example.com",
        .path   = "/very/long/path/that/will/not/fit",
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len),
              HTTP_REQUEST_ERR_OVERFLOW);
    ASSERT_EQ(len, (size_t)0);
}

TEST(test_range_get_overflow)
{
    uint8_t buf[10];
    size_t len;
    ASSERT_EQ(http_request_build_range_get(buf, sizeof(buf),
              "host.io", "/fw", 0, 0, &len),
              HTTP_REQUEST_ERR_OVERFLOW);
}

/* ---------- NULL guards ---------- */

TEST(test_build_null_buf)
{
    size_t len;
    http_request_cfg_t cfg = { .path = "/" };
    ASSERT_EQ(http_request_build(NULL, 256, &cfg, &len), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_build_null_cfg)
{
    uint8_t buf[64];
    size_t len;
    ASSERT_EQ(http_request_build(buf, sizeof(buf), NULL, &len), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_build_null_outlen)
{
    uint8_t buf[64];
    http_request_cfg_t cfg = { .path = "/" };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, NULL), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_build_null_path)
{
    uint8_t buf[64];
    size_t len;
    http_request_cfg_t cfg = { .path = NULL };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_range_null_host)
{
    uint8_t buf[64];
    size_t len;
    ASSERT_EQ(http_request_build_range_get(buf, sizeof(buf),
              NULL, "/fw", 0, 0, &len), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_range_null_path)
{
    uint8_t buf[64];
    size_t len;
    ASSERT_EQ(http_request_build_range_get(buf, sizeof(buf),
              "host", NULL, 0, 0, &len), HTTP_REQUEST_ERR_PARAM);
}

/* ---------- error enum values ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)HTTP_REQUEST_OK);
    ASSERT_EQ(-1, (int)HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(-2, (int)HTTP_REQUEST_ERR_OVERFLOW);
}

/* ---------- no host ---------- */

TEST(test_no_host)
{
    uint8_t buf[256];
    size_t len;
    http_request_cfg_t cfg = {
        .method = HTTP_METHOD_GET,
        .path   = "/local",
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len), HTTP_REQUEST_OK);
    ASSERT_TRUE(!contains(buf, len, "Host:"));
    ASSERT_TRUE(contains(buf, len, "GET /local HTTP/1.1\r\n"));
}

/* ---------- skip NULL headers gracefully ---------- */

TEST(test_null_header_name_skipped)
{
    uint8_t buf[256];
    size_t len;
    http_header_t hdrs[] = {
        { .name = NULL, .value = "val" },
        { .name = "Good", .value = "yes" },
    };
    http_request_cfg_t cfg = {
        .method       = HTTP_METHOD_GET,
        .path         = "/",
        .headers      = hdrs,
        .header_count = 2,
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len), HTTP_REQUEST_OK);
    ASSERT_TRUE(contains(buf, len, "Good: yes\r\n"));
}

/* ---------- line breaks must not reach the wire ---------- */

static http_request_err_t build_with(const char *host, const char *path,
                                     const char *name, const char *value,
                                     size_t *len)
{
    static uint8_t buf[512];
    http_header_t hdrs[] = { { .name = name, .value = value } };
    http_request_cfg_t cfg = {
        .method       = HTTP_METHOD_GET,
        .host         = host,
        .path         = path,
        .headers      = hdrs,
        .header_count = 1,
    };
    return http_request_build(buf, sizeof(buf), &cfg, len);
}

TEST(test_path_cannot_add_a_second_request)
{
    size_t len = 99;
    ASSERT_EQ(build_with("h", "/a HTTP/1.1\r\nHost: h\r\n\r\nGET /evil",
                         "N", "v", &len), HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(len, (size_t)0);
}

TEST(test_path_refuses_cr_lf_and_space)
{
    size_t len;
    ASSERT_EQ(build_with("h", "/a\rb", "N", "v", &len), HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(build_with("h", "/a\nb", "N", "v", &len), HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(build_with("h", "/a b", "N", "v", &len), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_host_refuses_cr_lf)
{
    size_t len;
    ASSERT_EQ(build_with("h\r\nX: y", "/", "N", "v", &len),
              HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(build_with("h\ry", "/", "N", "v", &len), HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(build_with("h\ny", "/", "N", "v", &len), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_header_cannot_add_a_header)
{
    size_t len;
    ASSERT_EQ(build_with("h", "/", "N", "1\r\nX-Injected: yes", &len),
              HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(build_with("h", "/", "N\r\nX-Injected: yes", "1", &len),
              HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(build_with("h", "/", "N", "a\nb", &len), HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(build_with("h", "/", "N\ry", "b", &len), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_later_header_is_checked_too)
{
    uint8_t buf[256];
    size_t len;
    http_header_t hdrs[] = {
        { .name = "Good", .value = "yes" },
        { .name = NULL,   .value = "skipped\r\n" },
        { .name = "Bad",  .value = "a\r\nb" },
    };
    http_request_cfg_t cfg = {
        .method       = HTTP_METHOD_GET,
        .path         = "/",
        .headers      = hdrs,
        .header_count = 3,
    };
    ASSERT_EQ(http_request_build(buf, sizeof(buf), &cfg, &len),
              HTTP_REQUEST_ERR_PARAM);
}

TEST(test_range_get_refuses_line_breaks)
{
    uint8_t buf[256];
    size_t len;
    ASSERT_EQ(http_request_build_range_get(buf, sizeof(buf), "h",
              "/fw\r\nX: y", 0, 0, &len), HTTP_REQUEST_ERR_PARAM);
    ASSERT_EQ(http_request_build_range_get(buf, sizeof(buf), "h\r\nX: y",
              "/fw", 0, 0, &len), HTTP_REQUEST_ERR_PARAM);
}

TEST(test_spaces_stay_legal_in_host_and_header_value)
{
    size_t len;
    /* Only the request line is split by spaces (RFC 9112 section 3). */
    ASSERT_EQ(build_with("h", "/", "User-Agent", "a b c", &len),
              HTTP_REQUEST_OK);
}

TEST_MAIN("http_request")
    RUN(test_path_cannot_add_a_second_request);
    RUN(test_path_refuses_cr_lf_and_space);
    RUN(test_host_refuses_cr_lf);
    RUN(test_header_cannot_add_a_header);
    RUN(test_later_header_is_checked_too);
    RUN(test_range_get_refuses_line_breaks);
    RUN(test_spaces_stay_legal_in_host_and_header_value);
    RUN(test_simple_get);
    RUN(test_post_with_body);
    RUN(test_head_method);
    RUN(test_put_method);
    RUN(test_multiple_headers);
    RUN(test_range_get_open);
    RUN(test_range_get_bounded);
    RUN(test_range_get_from_zero);
    RUN(test_build_overflow);
    RUN(test_range_get_overflow);
    RUN(test_build_null_buf);
    RUN(test_build_null_cfg);
    RUN(test_build_null_outlen);
    RUN(test_build_null_path);
    RUN(test_range_null_host);
    RUN(test_range_null_path);
    RUN(test_error_codes);
    RUN(test_no_host);
    RUN(test_null_header_name_skipped);
TEST_MAIN_END()
