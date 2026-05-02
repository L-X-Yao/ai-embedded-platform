// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "http_parser.h"
#include <string.h>
#include <stdio.h>

TEST(test_parse_get_request)
{
    const char *data = "GET /index.html HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "Connection: close\r\n"
                       "\r\n";
    http_parser_request_t req;
    ASSERT_EQ(http_parser_parse_request(data, strlen(data), &req), HTTP_PARSER_OK);
    ASSERT_EQ(req.method_len, 3);
    ASSERT_MEMEQ(req.method, "GET");
    ASSERT_EQ(req.uri_len, 11);
    ASSERT_MEMEQ(req.uri, "/index.html");
    ASSERT_EQ(req.version_major, 1);
    ASSERT_EQ(req.version_minor, 1);
    ASSERT_EQ(req.header_count, 2);
    ASSERT_EQ(req.body_len, 0);
}

TEST(test_parse_post_with_body)
{
    const char *data = "POST /api/data HTTP/1.1\r\n"
                       "Content-Length: 13\r\n"
                       "\r\n"
                       "Hello, World!";
    http_parser_request_t req;
    ASSERT_EQ(http_parser_parse_request(data, strlen(data), &req), HTTP_PARSER_OK);
    ASSERT_MEMEQ(req.method, "POST");
    ASSERT_EQ(req.body_len, 13);
    ASSERT_MEMEQ(req.body, "Hello, World!");
}

TEST(test_parse_response_200)
{
    const char *data = "HTTP/1.1 200 OK\r\n"
                       "Content-Length: 5\r\n"
                       "Content-Type: text/plain\r\n"
                       "\r\n"
                       "Hello";
    http_parser_response_t resp;
    ASSERT_EQ(http_parser_parse_response(data, strlen(data), &resp), HTTP_PARSER_OK);
    ASSERT_EQ(resp.status_code, 200);
    ASSERT_EQ(resp.reason_len, 2);
    ASSERT_MEMEQ(resp.reason, "OK");
    ASSERT_EQ(resp.header_count, 2);
    ASSERT_EQ(resp.body_len, 5);
    ASSERT_MEMEQ(resp.body, "Hello");
}

TEST(test_parse_response_404)
{
    const char *data = "HTTP/1.1 404 Not Found\r\n"
                       "\r\n";
    http_parser_response_t resp;
    ASSERT_EQ(http_parser_parse_response(data, strlen(data), &resp), HTTP_PARSER_OK);
    ASSERT_EQ(resp.status_code, 404);
    ASSERT_EQ(resp.body_len, 0);
}

TEST(test_incomplete_request)
{
    const char *data = "GET /index.html HTTP/1.1\r\n"
                       "Host: example";
    http_parser_request_t req;
    ASSERT_EQ(http_parser_parse_request(data, strlen(data), &req), HTTP_PARSER_ERR_INCOMPLETE);
}

TEST(test_incomplete_body)
{
    const char *data = "POST /api HTTP/1.1\r\n"
                       "Content-Length: 100\r\n"
                       "\r\n"
                       "short";
    http_parser_request_t req;
    ASSERT_EQ(http_parser_parse_request(data, strlen(data), &req), HTTP_PARSER_ERR_INCOMPLETE);
}

TEST(test_find_header)
{
    const char *data = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "Accept: text/html\r\n"
                       "\r\n";
    http_parser_request_t req;
    http_parser_parse_request(data, strlen(data), &req);

    const http_parser_header_t *hdr = NULL;
    ASSERT_EQ(http_parser_find_header(req.headers, req.header_count, "Host", &hdr), HTTP_PARSER_OK);
    ASSERT_EQ(hdr->value_len, 11);
    ASSERT_MEMEQ(hdr->value, "example.com");

    ASSERT_EQ(http_parser_find_header(req.headers, req.header_count, "host", &hdr), HTTP_PARSER_OK);
}

TEST(test_find_header_missing)
{
    const char *data = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "\r\n";
    http_parser_request_t req;
    http_parser_parse_request(data, strlen(data), &req);

    const http_parser_header_t *hdr = NULL;
    ASSERT_EQ(http_parser_find_header(req.headers, req.header_count, "X-Missing", &hdr),
              HTTP_PARSER_ERR_NOT_FOUND);
}

TEST(test_null_safety)
{
    ASSERT_EQ(http_parser_parse_request(NULL, 0, NULL), HTTP_PARSER_ERR_PARAM);
    ASSERT_EQ(http_parser_parse_response(NULL, 0, NULL), HTTP_PARSER_ERR_PARAM);
    ASSERT_EQ(http_parser_find_header(NULL, 0, NULL, NULL), HTTP_PARSER_ERR_PARAM);
}

TEST(test_content_length)
{
    http_parser_header_t hdr = { .name = "Content-Length", .name_len = 14,
                          .value = "42", .value_len = 2 };
    ASSERT_EQ(http_parser_get_content_length(&hdr, 1), 42);
}

TEST(test_malformed_request)
{
    http_parser_request_t req;
    const char *no_space = "GETONLY\r\n\r\n";
    ASSERT_EQ(http_parser_parse_request(no_space, strlen(no_space), &req),
              HTTP_PARSER_ERR_MALFORMED);
    const char *bad_ver = "GET / HTTP/1X1\r\n\r\n";
    ASSERT_EQ(http_parser_parse_request(bad_ver, strlen(bad_ver), &req),
              HTTP_PARSER_ERR_MALFORMED);
}

TEST(test_too_many_headers)
{
    char buf[2048];
    int pos = snprintf(buf, sizeof(buf), "GET / HTTP/1.1\r\n");
    for (int i = 0; i < HTTP_PARSER_MAX_HEADERS + 1; i++)
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                        "X-Hdr-%d: val%d\r\n", i, i);
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "\r\n");
    http_parser_request_t req;
    ASSERT_EQ(http_parser_parse_request(buf, (size_t)pos, &req), HTTP_PARSER_ERR_TOO_MANY);
}

TEST_MAIN("http_parser")
    RUN(test_parse_get_request);
    RUN(test_parse_post_with_body);
    RUN(test_parse_response_200);
    RUN(test_parse_response_404);
    RUN(test_incomplete_request);
    RUN(test_incomplete_body);
    RUN(test_find_header);
    RUN(test_find_header_missing);
    RUN(test_null_safety);
    RUN(test_content_length);
    RUN(test_malformed_request);
    RUN(test_too_many_headers);
TEST_MAIN_END()
