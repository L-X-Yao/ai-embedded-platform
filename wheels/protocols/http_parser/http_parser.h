/*
 * http_parser — lightweight HTTP/1.1 request/response parser
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
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
    HTTP_OK           =  0,
    HTTP_ERR_PARAM    = -1,
    HTTP_ERR_INCOMPLETE = -2,
    HTTP_ERR_MALFORMED  = -3,
    HTTP_ERR_TOO_MANY   = -4,
    HTTP_ERR_NOT_FOUND  = -5,
} http_err_t;

typedef struct {
    const char *name;
    size_t      name_len;
    const char *value;
    size_t      value_len;
} http_header_t;

#ifndef HTTP_MAX_HEADERS
#define HTTP_MAX_HEADERS 16
#endif

typedef struct {
    const char    *method;
    size_t         method_len;
    const char    *uri;
    size_t         uri_len;
    int            version_major;
    int            version_minor;
    http_header_t  headers[HTTP_MAX_HEADERS];
    int            header_count;
    const char    *body;
    size_t         body_len;
    size_t         total_consumed;
} http_request_t;

typedef struct {
    int            status_code;
    const char    *reason;
    size_t         reason_len;
    int            version_major;
    int            version_minor;
    http_header_t  headers[HTTP_MAX_HEADERS];
    int            header_count;
    const char    *body;
    size_t         body_len;
    size_t         total_consumed;
} http_response_t;

/** @thread_safe: yes (pure function, no state) */
http_err_t http_parse_request(const char *data, size_t len,
                              http_request_t *req);

http_err_t http_parse_response(const char *data, size_t len,
                               http_response_t *resp);

http_err_t http_find_header(const http_header_t *headers, int count,
                            const char *name, const http_header_t **out);

int http_get_content_length(const http_header_t *headers, int count);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_PARSER_H */
