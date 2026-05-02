// SPDX-License-Identifier: MIT
/*
 * http_parser — lightweight HTTP/1.1 request/response parser
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Zero-copy: parsed fields point into the caller's input buffer.
 * Supports chunked parsing (feed partial data).
 * Pure C99, no heap, fully reentrant per-context.
 * Thread safety: all functions are pure / reentrant (zero-copy, no global state).
 */

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HTTP_PARSER_OK           =  0,
    HTTP_PARSER_ERR_PARAM    = -1,
    HTTP_PARSER_ERR_INCOMPLETE = -2,
    HTTP_PARSER_ERR_MALFORMED  = -3,
    HTTP_PARSER_ERR_TOO_MANY   = -4,
    HTTP_PARSER_ERR_NOT_FOUND  = -5,
} http_parser_err_t;

typedef struct {
    const char *name;
    size_t      name_len;
    const char *value;
    size_t      value_len;
} http_parser_header_t;

#ifndef HTTP_PARSER_MAX_HEADERS
#define HTTP_PARSER_MAX_HEADERS 16
#endif

typedef struct {
    const char    *method;
    size_t         method_len;
    const char    *uri;
    size_t         uri_len;
    int            version_major;
    int            version_minor;
    http_parser_header_t  headers[HTTP_PARSER_MAX_HEADERS];
    int            header_count;
    const char    *body;
    size_t         body_len;
    size_t         total_consumed;
} http_parser_request_t;

typedef struct {
    int            status_code;
    const char    *reason;
    size_t         reason_len;
    int            version_major;
    int            version_minor;
    http_parser_header_t  headers[HTTP_PARSER_MAX_HEADERS];
    int            header_count;
    const char    *body;
    size_t         body_len;
    size_t         total_consumed;
} http_parser_response_t;

/** @thread_safe: yes (pure function, no state) */
http_parser_err_t http_parser_parse_request(const char *data, size_t len,
                              http_parser_request_t *req);

/** Parse an HTTP/1.1 response from raw data into a response struct. */
http_parser_err_t http_parser_parse_response(const char *data, size_t len,
                               http_parser_response_t *resp);

/** Find a header by name (case-insensitive) and return a pointer to it. */
http_parser_err_t http_parser_find_header(const http_parser_header_t *headers, int count,
                            const char *name, const http_parser_header_t **out);

/** Return the Content-Length value from headers, or -1 if absent/invalid. */
int http_parser_get_content_length(const http_parser_header_t *headers, int count);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_PARSER_H */
