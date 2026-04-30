/*
 * http_parser — lightweight HTTP/1.1 request/response parser
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */
#include "http_parser.h"
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

static http_err_t parse_headers(const char *data, size_t len,
                                http_header_t *headers, int max_headers,
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
            return HTTP_OK;
        }

        const char *crlf = find_crlf(p, (size_t)(end - p));
        if (!crlf)
            return HTTP_ERR_INCOMPLETE;

        const char *colon = NULL;
        for (const char *c = p; c < crlf; c++) {
            if (*c == ':') { colon = c; break; }
        }
        if (!colon)
            return HTTP_ERR_MALFORMED;

        if (*count >= max_headers)
            return HTTP_ERR_TOO_MANY;

        headers[*count].name = p;
        headers[*count].name_len = (size_t)(colon - p);

        const char *val = colon + 1;
        while (val < crlf && *val == ' ') val++;
        headers[*count].value = val;
        headers[*count].value_len = (size_t)(crlf - val);
        (*count)++;

        p = crlf + 2;
    }

    return HTTP_ERR_INCOMPLETE;
}

http_err_t http_parse_request(const char *data, size_t len,
                              http_request_t *req)
{
    if (!data || !req)
        return HTTP_ERR_PARAM;

    memset(req, 0, sizeof(*req));

    const char *crlf = find_crlf(data, len);
    if (!crlf)
        return HTTP_ERR_INCOMPLETE;

    const char *p = data;
    req->method = p;
    while (p < crlf && *p != ' ') p++;
    if (p >= crlf) return HTTP_ERR_MALFORMED;
    req->method_len = (size_t)(p - req->method);

    p++;
    req->uri = p;
    while (p < crlf && *p != ' ') p++;
    if (p >= crlf) return HTTP_ERR_MALFORMED;
    req->uri_len = (size_t)(p - req->uri);

    p++;
    if (p + 8 > crlf) return HTTP_ERR_MALFORMED;
    if (memcmp(p, "HTTP/", 5) != 0) return HTTP_ERR_MALFORMED;
    req->version_major = p[5] - '0';
    req->version_minor = p[7] - '0';

    p = crlf + 2;
    const char *body_start = NULL;
    size_t hdr_consumed = 0;
    http_err_t err = parse_headers(p, (size_t)(data + len - p),
                                   req->headers, HTTP_MAX_HEADERS,
                                   &req->header_count, &body_start,
                                   &hdr_consumed);
    if (err != HTTP_OK)
        return err;

    size_t header_end = (size_t)(body_start - data);
    int content_length = http_get_content_length(req->headers, req->header_count);
    if (content_length > 0) {
        if (header_end + (size_t)content_length > len)
            return HTTP_ERR_INCOMPLETE;
        req->body = body_start;
        req->body_len = (size_t)content_length;
        req->total_consumed = header_end + (size_t)content_length;
    } else {
        req->body = NULL;
        req->body_len = 0;
        req->total_consumed = header_end;
    }

    return HTTP_OK;
}

http_err_t http_parse_response(const char *data, size_t len,
                               http_response_t *resp)
{
    if (!data || !resp)
        return HTTP_ERR_PARAM;

    memset(resp, 0, sizeof(*resp));

    const char *crlf = find_crlf(data, len);
    if (!crlf)
        return HTTP_ERR_INCOMPLETE;

    const char *p = data;
    if (p + 9 > crlf) return HTTP_ERR_MALFORMED;
    if (memcmp(p, "HTTP/", 5) != 0) return HTTP_ERR_MALFORMED;
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
    if (p < crlf && *p == ' ') p++;
    resp->reason = p;
    resp->reason_len = (size_t)(crlf - p);

    p = crlf + 2;
    const char *body_start = NULL;
    size_t hdr_consumed = 0;
    http_err_t err = parse_headers(p, (size_t)(data + len - p),
                                   resp->headers, HTTP_MAX_HEADERS,
                                   &resp->header_count, &body_start,
                                   &hdr_consumed);
    if (err != HTTP_OK)
        return err;

    size_t header_end = (size_t)(body_start - data);
    int content_length = http_get_content_length(resp->headers, resp->header_count);
    if (content_length > 0) {
        if (header_end + (size_t)content_length > len)
            return HTTP_ERR_INCOMPLETE;
        resp->body = body_start;
        resp->body_len = (size_t)content_length;
        resp->total_consumed = header_end + (size_t)content_length;
    } else {
        resp->body = NULL;
        resp->body_len = 0;
        resp->total_consumed = header_end;
    }

    return HTTP_OK;
}

http_err_t http_find_header(const http_header_t *headers, int count,
                            const char *name, const http_header_t **out)
{
    if (!headers || !name || !out)
        return HTTP_ERR_PARAM;

    size_t nlen = strlen(name);
    for (int i = 0; i < count; i++) {
        if (ci_equal(headers[i].name, headers[i].name_len, name, nlen)) {
            *out = &headers[i];
            return HTTP_OK;
        }
    }
    return HTTP_ERR_MALFORMED;
}

int http_get_content_length(const http_header_t *headers, int count)
{
    if (!headers) return -1;
    for (int i = 0; i < count; i++) {
        if (ci_equal(headers[i].name, headers[i].name_len,
                     "content-length", 14)) {
            int val = 0;
            for (size_t j = 0; j < headers[i].value_len; j++) {
                char c = headers[i].value[j];
                if (c < '0' || c > '9') return -1;
                if (val > 214748364 || (val == 214748364 && (c - '0') > 7))
                    return -1;
                val = val * 10 + (c - '0');
            }
            return val;
        }
    }
    return -1;
}
