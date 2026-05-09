// SPDX-License-Identifier: MIT
/*
 * http_request -- HTTP/1.1 request builder (zero-alloc, buffer-based)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "http_request.h"
#include "contract.h"
#include <string.h>
#include <stdio.h>

static const char *method_str(http_method_t m)
{
    switch (m) {
        case HTTP_METHOD_GET:  return "GET";
        case HTTP_METHOD_POST: return "POST";
        case HTTP_METHOD_PUT:  return "PUT";
        case HTTP_METHOD_HEAD: return "HEAD";
        default:               return "GET";
    }
}

http_request_err_t http_request_build(uint8_t *buf, size_t cap,
                                      const http_request_cfg_t *cfg,
                                      size_t *out_len)
{
    REQUIRE(buf != NULL);
    REQUIRE(cfg != NULL);
    REQUIRE(out_len != NULL);
    if (!buf || !cfg || !out_len || !cfg->path) {
        return HTTP_REQUEST_ERR_PARAM;
    }

    size_t pos = 0;
    const char *mstr = method_str(cfg->method);
    size_t mlen = strlen(mstr);
    size_t plen = strlen(cfg->path);

    /* Request line: "METHOD path HTTP/1.1\r\n" */
    size_t rline_len = mlen + 1 + plen + 11;
    if (pos + rline_len > cap) { goto overflow; }
    memcpy(buf + pos, mstr, mlen); pos += mlen;
    buf[pos++] = ' ';
    memcpy(buf + pos, cfg->path, plen); pos += plen;
    memcpy(buf + pos, " HTTP/1.1\r\n", 11); pos += 11;

    /* Host header */
    if (cfg->host) {
        size_t hlen = strlen(cfg->host);
        size_t line_len = 6 + hlen + 2;
        if (pos + line_len > cap) { goto overflow; }
        memcpy(buf + pos, "Host: ", 6); pos += 6;
        memcpy(buf + pos, cfg->host, hlen); pos += hlen;
        memcpy(buf + pos, "\r\n", 2); pos += 2;
    }

    /* Custom headers */
    for (size_t i = 0; i < cfg->header_count; i++) {
        if (!cfg->headers || !cfg->headers[i].name || !cfg->headers[i].value) {
            continue;
        }
        size_t nlen = strlen(cfg->headers[i].name);
        size_t vlen = strlen(cfg->headers[i].value);
        size_t line_len = nlen + 2 + vlen + 2;
        if (pos + line_len > cap) { goto overflow; }
        memcpy(buf + pos, cfg->headers[i].name, nlen); pos += nlen;
        memcpy(buf + pos, ": ", 2); pos += 2;
        memcpy(buf + pos, cfg->headers[i].value, vlen); pos += vlen;
        memcpy(buf + pos, "\r\n", 2); pos += 2;
    }

    /* Content-Length for body */
    if (cfg->body && cfg->body_len > 0) {
        char cl[48];
        int n = snprintf(cl, sizeof(cl), "Content-Length: %u\r\n",
                         (unsigned)cfg->body_len);
        if (n < 0 || (size_t)n >= sizeof(cl)) { goto overflow; }
        if (pos + (size_t)n > cap) { goto overflow; }
        memcpy(buf + pos, cl, (size_t)n); pos += (size_t)n;
    }

    /* End of headers */
    if (pos + 2 > cap) { goto overflow; }
    memcpy(buf + pos, "\r\n", 2); pos += 2;

    /* Body */
    if (cfg->body && cfg->body_len > 0) {
        if (pos + cfg->body_len > cap) { goto overflow; }
        memcpy(buf + pos, cfg->body, cfg->body_len);
        pos += cfg->body_len;
    }

    *out_len = pos;
    return HTTP_REQUEST_OK;

overflow:
    *out_len = 0;
    return HTTP_REQUEST_ERR_OVERFLOW;
}

http_request_err_t http_request_build_range_get(uint8_t *buf, size_t cap,
                                                const char *host,
                                                const char *path,
                                                size_t offset, size_t end,
                                                size_t *out_len)
{
    REQUIRE(buf != NULL);
    REQUIRE(host != NULL);
    REQUIRE(path != NULL);
    REQUIRE(out_len != NULL);
    if (!buf || !host || !path || !out_len) {
        return HTTP_REQUEST_ERR_PARAM;
    }

    char range_val[64];
    int n;
    if (end > 0 && end >= offset) {
        n = snprintf(range_val, sizeof(range_val), "bytes=%u-%u",
                     (unsigned)offset, (unsigned)end);
    } else {
        n = snprintf(range_val, sizeof(range_val), "bytes=%u-",
                     (unsigned)offset);
    }
    if (n < 0 || (size_t)n >= sizeof(range_val)) {
        return HTTP_REQUEST_ERR_OVERFLOW;
    }

    http_header_t hdrs[1] = {
        { .name = "Range", .value = range_val },
    };

    http_request_cfg_t cfg = {
        .method       = HTTP_METHOD_GET,
        .host         = host,
        .path         = path,
        .headers      = hdrs,
        .header_count = 1,
        .body         = NULL,
        .body_len     = 0,
    };

    return http_request_build(buf, cap, &cfg, out_len);
}
