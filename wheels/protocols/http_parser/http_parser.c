// SPDX-License-Identifier: MIT
/*
 * http_parser — lightweight HTTP/1.1 request/response parser
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "http_parser.h"
#include "contract.h"
#include <string.h>

static const char *find_crlf(const char *s, size_t len)
{
    for (size_t i = 0; i + 1 < len; i++) {
        if (s[i] == '\r' && s[i + 1] == '\n')
            return &s[i];
    }
    return NULL;
}

static bool ci_equal(const char *a, size_t alen, const char *b, size_t blen)
{
    if (alen != blen) return false;
    for (size_t i = 0; i < alen; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return false;
    }
    return true;
}

/* One Content-Length value: 1*DIGIT that fits an int, else -1. */
static int length_value(const char *value, size_t len)
{
    int val = 0;

    if (len == 0) return -1;
    for (size_t j = 0; j < len; j++) {
        char c = value[j];
        if (c < '0' || c > '9') return -1;
        if (val > 214748364 || (val == 214748364 && (c - '0') > 7))
            return -1;
        val = val * 10 + (c - '0');
    }
    return val;
}

/* Check every Content-Length field: *length is -1 when there is none. An
 * invalid value, or two that differ, cannot frame a message (RFC 7230
 * section 3.3.3). Repeats of one value count as that value (section 3.3.2). */
static http_parser_err_t scan_content_length(const http_parser_header_t *headers,
                                             int count, int *length)
{
    *length = -1;
    for (int i = 0; i < count; i++) {
        if (!ci_equal(headers[i].name, headers[i].name_len,
                      "content-length", 14))
            continue;
        int val = length_value(headers[i].value, headers[i].value_len);
        if (val < 0 || (*length >= 0 && val != *length))
            return HTTP_PARSER_ERR_MALFORMED;
        *length = val;
    }
    return HTTP_PARSER_OK;
}

/* RFC 7230 section 3.3.3 item 3: Transfer-Encoding frames the body and overrides
 * Content-Length, and this parser decodes no transfer coding. */
static bool has_transfer_encoding(const http_parser_header_t *headers, int count)
{
    for (int i = 0; i < count; i++) {
        if (ci_equal(headers[i].name, headers[i].name_len,
                     "transfer-encoding", 17))
            return true;
    }
    return false;
}

static http_parser_err_t parse_headers(const char *data, size_t len,
                                http_parser_header_t *headers, int max_headers,
                                int *count, const char **body_start,
                                size_t *consumed)
{
    const char *p = data;
    const char *end = data + len;
    *count = 0;

    while (p < end) {
        if (p + 1 < end && p[0] == '\r' && p[1] == '\n') {
            *body_start = p + 2;
            *consumed = (size_t)(*body_start - data);
            return HTTP_PARSER_OK;
        }

        const char *crlf = find_crlf(p, (size_t)(end - p));
        if (!crlf)
            return HTTP_PARSER_ERR_INCOMPLETE;

        const char *colon = NULL;
        for (const char *c = p; c < crlf; c++) {
            if (*c == ':') { colon = c; break; }
            /* A name holds no whitespace, least of all before the colon
             * (RFC 7230 section 3.2.4): it would hide the header */
            if (*c == ' ' || *c == '\t') return HTTP_PARSER_ERR_MALFORMED;
        }
        if (!colon)
            return HTTP_PARSER_ERR_MALFORMED;

        if (*count >= max_headers)
            return HTTP_PARSER_ERR_TOO_MANY;

        headers[*count].name = p;
        headers[*count].name_len = (size_t)(colon - p);

        const char *val = colon + 1;
        while (val < crlf && (*val == ' ' || *val == '\t')) val++;
        const char *val_end = crlf;
        while (val_end > val && (val_end[-1] == ' ' || val_end[-1] == '\t'))
            val_end--;
        headers[*count].value = val;
        headers[*count].value_len = (size_t)(val_end - val);
        (*count)++;

        p = crlf + 2;
    }

    return HTTP_PARSER_ERR_INCOMPLETE;
}

http_parser_err_t http_parser_parse_request(const char *data, size_t len,
                              http_parser_request_t *req)
{
    if (!data || !req)
        return HTTP_PARSER_ERR_PARAM;

    memset(req, 0, sizeof(*req));

    const char *crlf = find_crlf(data, len);
    if (!crlf)
        return HTTP_PARSER_ERR_INCOMPLETE;

    const char *p = data;
    req->method = p;
    while (p < crlf && *p != ' ') p++;
    if (p >= crlf) return HTTP_PARSER_ERR_MALFORMED;
    req->method_len = (size_t)(p - req->method);

    p++;
    req->uri = p;
    while (p < crlf && *p != ' ') p++;
    if (p >= crlf) return HTTP_PARSER_ERR_MALFORMED;
    req->uri_len = (size_t)(p - req->uri);

    p++;
    if (p + 8 > crlf) return HTTP_PARSER_ERR_MALFORMED;
    if (memcmp(p, "HTTP/", 5) != 0) return HTTP_PARSER_ERR_MALFORMED;
    if (p[5] < '0' || p[5] > '9' || p[6] != '.' || p[7] < '0' || p[7] > '9')
        return HTTP_PARSER_ERR_MALFORMED;
    req->version_major = p[5] - '0';
    req->version_minor = p[7] - '0';

    p = crlf + 2;
    const char *body_start = NULL;
    size_t hdr_consumed = 0;
    http_parser_err_t err = parse_headers(p, (size_t)(data + len - p),
                                   req->headers, HTTP_PARSER_MAX_HEADERS,
                                   &req->header_count, &body_start,
                                   &hdr_consumed);
    if (err != HTTP_PARSER_OK)
        return err;

    size_t header_end = (size_t)(body_start - data);
    if (has_transfer_encoding(req->headers, req->header_count)) {
        req->total_consumed = header_end;
        return HTTP_PARSER_ERR_TRANSFER_ENCODING;
    }
    int content_length = -1;
    err = scan_content_length(req->headers, req->header_count, &content_length);
    if (err != HTTP_PARSER_OK)
        return err;
    if (content_length > 0) {
        if (header_end + (size_t)content_length > len)
            return HTTP_PARSER_ERR_INCOMPLETE;
        req->body = body_start;
        req->body_len = (size_t)content_length;
        req->total_consumed = header_end + (size_t)content_length;
    } else {
        req->body = NULL;
        req->body_len = 0;
        req->total_consumed = header_end;
    }

    ENSURE(req->total_consumed <= len);
    return HTTP_PARSER_OK;
}

/* RFC 7230 section 3.3.3 item 1: these replies end at the blank line whatever
 * their header fields say, so more data can never complete them. */
static bool status_has_no_body(int status)
{
    return (status >= 100 && status <= 199) || status == 204 || status == 304;
}

static http_parser_err_t parse_response(const char *data, size_t len,
                                        http_parser_response_t *resp,
                                        bool to_head)
{
    if (!data || !resp)
        return HTTP_PARSER_ERR_PARAM;

    memset(resp, 0, sizeof(*resp));

    const char *crlf = find_crlf(data, len);
    if (!crlf)
        return HTTP_PARSER_ERR_INCOMPLETE;

    const char *p = data;
    if (p + 9 > crlf) return HTTP_PARSER_ERR_MALFORMED;
    if (memcmp(p, "HTTP/", 5) != 0) return HTTP_PARSER_ERR_MALFORMED;
    if (p[5] < '0' || p[5] > '9' || p[6] != '.' || p[7] < '0' || p[7] > '9')
        return HTTP_PARSER_ERR_MALFORMED;
    resp->version_major = p[5] - '0';
    resp->version_minor = p[7] - '0';

    p += 9;
    resp->status_code = 0;
    int digits = 0;
    while (p < crlf && *p >= '0' && *p <= '9' && digits < 3) {
        resp->status_code = resp->status_code * 10 + (*p - '0');
        p++;
        digits++;
    }
    if (digits != 3) return HTTP_PARSER_ERR_MALFORMED;
    /* Reject 4+ digit status codes (e.g. "1234") */
    if (p < crlf && *p >= '0' && *p <= '9') return HTTP_PARSER_ERR_MALFORMED;
    if (p < crlf && *p == ' ') p++;
    resp->reason = p;
    resp->reason_len = (size_t)(crlf - p);

    p = crlf + 2;
    const char *body_start = NULL;
    size_t hdr_consumed = 0;
    http_parser_err_t err = parse_headers(p, (size_t)(data + len - p),
                                   resp->headers, HTTP_PARSER_MAX_HEADERS,
                                   &resp->header_count, &body_start,
                                   &hdr_consumed);
    if (err != HTTP_PARSER_OK)
        return err;

    size_t header_end = (size_t)(body_start - data);
    int content_length = -1;
    if (!to_head && !status_has_no_body(resp->status_code)) {
        if (has_transfer_encoding(resp->headers, resp->header_count)) {
            resp->total_consumed = header_end;
            return HTTP_PARSER_ERR_TRANSFER_ENCODING;
        }
        err = scan_content_length(resp->headers, resp->header_count,
                                  &content_length);
        if (err != HTTP_PARSER_OK)
            return err;
    }
    if (content_length > 0) {
        if (header_end + (size_t)content_length > len)
            return HTTP_PARSER_ERR_INCOMPLETE;
        resp->body = body_start;
        resp->body_len = (size_t)content_length;
        resp->total_consumed = header_end + (size_t)content_length;
    } else {
        resp->body = NULL;
        resp->body_len = 0;
        resp->total_consumed = header_end;
    }

    ENSURE(resp->total_consumed <= len);
    return HTTP_PARSER_OK;
}

http_parser_err_t http_parser_parse_response(const char *data, size_t len,
                               http_parser_response_t *resp)
{
    return parse_response(data, len, resp, false);
}

http_parser_err_t http_parser_parse_response_to_head(const char *data, size_t len,
                               http_parser_response_t *resp)
{
    return parse_response(data, len, resp, true);
}

http_parser_err_t http_parser_find_header(const http_parser_header_t *headers, int count,
                            const char *name, const http_parser_header_t **out)
{
    if (!headers || !name || !out)
        return HTTP_PARSER_ERR_PARAM;

    size_t nlen = strlen(name);
    for (int i = 0; i < count; i++) {
        if (ci_equal(headers[i].name, headers[i].name_len, name, nlen)) {
            *out = &headers[i];
            return HTTP_PARSER_OK;
        }
    }
    return HTTP_PARSER_ERR_NOT_FOUND;
}

int http_parser_get_content_length(const http_parser_header_t *headers, int count)
{
    if (!headers) return -1;

    int length = -1;
    if (scan_content_length(headers, count, &length) != HTTP_PARSER_OK)
        return -1;
    return length;
}
