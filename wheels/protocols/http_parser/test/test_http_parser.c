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

/* Header blocks that must not frame a message. RFC 7230 3.3.3 item 4 makes an
   invalid or differing Content-Length an unrecoverable error, 3.3.2 gives
   Content-Length = 1*DIGIT, and 3.2.4 allows no whitespace before the colon. */
static const char *const bad_framing[] = {
    "Content-Length: abc\r\n",
    "Content-Length: 5a\r\n",
    "Content-Length: -5\r\n",
    "Content-Length: +5\r\n",
    "Content-Length: 99999999999\r\n",
    "Content-Length:\r\n",
    "Content-Length: 0\r\nContent-Length: 5\r\n",
    "Content-Length: 5\r\nContent-Length: 0\r\n",
    "Content-Length : 5\r\n",
    "Content-Length\t: 5\r\n",
    " Content-Length: 5\r\n",
};

TEST(test_request_refuses_bad_framing)
{
    for (size_t i = 0; i < sizeof(bad_framing) / sizeof(bad_framing[0]); i++) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "POST /a HTTP/1.1\r\n%s\r\nhello",
                         bad_framing[i]);
        http_parser_request_t req;
        ASSERT_EQ(http_parser_parse_request(buf, (size_t)n, &req),
                  HTTP_PARSER_ERR_MALFORMED);
    }
}

TEST(test_response_refuses_bad_framing)
{
    for (size_t i = 0; i < sizeof(bad_framing) / sizeof(bad_framing[0]); i++) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "HTTP/1.1 200 OK\r\n%s\r\nhello",
                         bad_framing[i]);
        http_parser_response_t resp;
        ASSERT_EQ(http_parser_parse_response(buf, (size_t)n, &resp),
                  HTTP_PARSER_ERR_MALFORMED);
    }
}

TEST(test_repeated_equal_content_length_frames_the_body)
{
    /* RFC 7230 3.3.2 lets a recipient keep one of several equal values */
    const char *data = "POST /a HTTP/1.1\r\n"
                       "Content-Length: 5\r\n"
                       "Content-Length: 5\r\n"
                       "\r\n"
                       "hello";
    http_parser_request_t req;
    ASSERT_EQ(http_parser_parse_request(data, strlen(data), &req), HTTP_PARSER_OK);
    ASSERT_EQ(req.body_len, 5U);
    ASSERT_EQ(req.total_consumed, strlen(data));
}

TEST(test_no_content_length_is_still_no_body)
{
    const char *data = "GET /a HTTP/1.1\r\nHost: h\r\n\r\n";
    http_parser_request_t req;
    ASSERT_EQ(http_parser_parse_request(data, strlen(data), &req), HTTP_PARSER_OK);
    ASSERT_EQ(req.body_len, 0U);
    ASSERT_EQ(http_parser_get_content_length(req.headers, req.header_count), -1);
}

TEST(test_trailing_whitespace_is_not_part_of_a_value)
{
    /* RFC 7230 section 3.2: OWS sits on both sides of field-value */
    static const char *const reqs[] = {
        "POST / HTTP/1.1\r\nContent-Length: 5 \r\n\r\nhello",
        "POST / HTTP/1.1\r\nContent-Length: 5\t\r\n\r\nhello",
        "POST / HTTP/1.1\r\nContent-Length:  5 \t \r\n\r\nhello",
    };

    for (size_t i = 0; i < sizeof(reqs) / sizeof(reqs[0]); i++) {
        http_parser_request_t req;
        ASSERT_EQ(http_parser_parse_request(reqs[i], strlen(reqs[i]), &req),
                  HTTP_PARSER_OK);
        ASSERT_EQ(req.body_len, 5);
        ASSERT_EQ(req.headers[0].value_len, 1);
    }
}

TEST(test_get_content_length_says_invalid_for_bad_values)
{
    http_parser_header_t two[2] = {
        { .name = "Content-Length", .name_len = 14, .value = "0", .value_len = 1 },
        { .name = "Content-Length", .name_len = 14, .value = "5", .value_len = 1 },
    };
    http_parser_header_t empty = {
        .name = "Content-Length", .name_len = 14, .value = "", .value_len = 0
    };
    ASSERT_EQ(http_parser_get_content_length(two, 2), -1);
    ASSERT_EQ(http_parser_get_content_length(&empty, 1), -1);
}

/* RFC 7230 3.3.3 item 1: a 1xx, 204 or 304 reply ends at the first blank line
   "regardless of the header fields present", so no more data can complete it */
TEST(test_bodiless_status_ends_at_the_blank_line)
{
    static const char *const replies[] = {
        "HTTP/1.1 100 Continue\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 101 Switching Protocols\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 199 Misc\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 204 No Content\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 304 Not Modified\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 304 Not Modified\r\nContent-Length: abc\r\n\r\n",
    };

    for (size_t i = 0; i < sizeof(replies) / sizeof(replies[0]); i++) {
        http_parser_response_t resp;
        ASSERT_EQ(http_parser_parse_response(replies[i], strlen(replies[i]), &resp),
                  HTTP_PARSER_OK);
        ASSERT_EQ(resp.body_len, 0U);
        ASSERT_TRUE(resp.body == NULL);
        ASSERT_EQ(resp.total_consumed, strlen(replies[i]));
    }
}

TEST(test_interim_reply_leaves_the_final_reply_to_parse)
{
    const char *data = "HTTP/1.1 100 Continue\r\n\r\n"
                       "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi";
    http_parser_response_t resp;
    ASSERT_EQ(http_parser_parse_response(data, strlen(data), &resp), HTTP_PARSER_OK);
    ASSERT_EQ(resp.status_code, 100);
    ASSERT_EQ(resp.total_consumed, 25U);

    const char *next = data + resp.total_consumed;
    ASSERT_EQ(http_parser_parse_response(next, strlen(next), &resp), HTTP_PARSER_OK);
    ASSERT_EQ(resp.status_code, 200);
    ASSERT_EQ(resp.body_len, 2U);
    ASSERT_MEMEQ(resp.body, "hi");
}

TEST(test_statuses_next_to_the_bodiless_ones_still_wait_for_the_body)
{
    static const char *const replies[] = {
        "HTTP/1.1 099 Low\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 203 Non-Authoritative\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 205 Reset Content\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 303 See Other\r\nContent-Length: 1000\r\n\r\n",
        "HTTP/1.1 305 Use Proxy\r\nContent-Length: 1000\r\n\r\n",
    };

    for (size_t i = 0; i < sizeof(replies) / sizeof(replies[0]); i++) {
        http_parser_response_t resp;
        ASSERT_EQ(http_parser_parse_response(replies[i], strlen(replies[i]), &resp),
                  HTTP_PARSER_ERR_INCOMPLETE);
    }
}

/* RFC 7230 3.3.3 item 1 again: only the caller knows the request was HEAD */
TEST(test_reply_to_head_ends_at_the_blank_line)
{
    const char *data = "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n";
    http_parser_response_t resp;
    ASSERT_EQ(http_parser_parse_response_to_head(data, strlen(data), &resp),
              HTTP_PARSER_OK);
    ASSERT_EQ(resp.status_code, 200);
    ASSERT_EQ(resp.body_len, 0U);
    ASSERT_TRUE(resp.body == NULL);
    ASSERT_EQ(resp.total_consumed, strlen(data));
    /* The headers still say how long a GET would have been */
    ASSERT_EQ(http_parser_get_content_length(resp.headers, resp.header_count), 1000);
}

TEST(test_reply_to_head_needs_its_blank_line)
{
    const char *data = "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n";
    http_parser_response_t resp;
    ASSERT_EQ(http_parser_parse_response_to_head(data, strlen(data), &resp),
              HTTP_PARSER_ERR_INCOMPLETE);
    ASSERT_EQ(http_parser_parse_response_to_head("HTTP/1.1 200 OK", 15, &resp),
              HTTP_PARSER_ERR_INCOMPLETE);
    ASSERT_EQ(http_parser_parse_response_to_head("nonsense\r\n\r\n", 12, &resp),
              HTTP_PARSER_ERR_MALFORMED);
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
    RUN(test_request_refuses_bad_framing);
    RUN(test_response_refuses_bad_framing);
    RUN(test_repeated_equal_content_length_frames_the_body);
    RUN(test_no_content_length_is_still_no_body);
    RUN(test_trailing_whitespace_is_not_part_of_a_value);
    RUN(test_get_content_length_says_invalid_for_bad_values);
    RUN(test_bodiless_status_ends_at_the_blank_line);
    RUN(test_interim_reply_leaves_the_final_reply_to_parse);
    RUN(test_statuses_next_to_the_bodiless_ones_still_wait_for_the_body);
    RUN(test_reply_to_head_ends_at_the_blank_line);
    RUN(test_reply_to_head_needs_its_blank_line);
TEST_MAIN_END()
