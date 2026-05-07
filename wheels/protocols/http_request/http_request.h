// SPDX-License-Identifier: MIT
/*
 * http_request -- HTTP/1.1 request builder (zero-alloc, buffer-based)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Builds GET/POST/PUT requests into a caller-provided buffer.
 * Designed for OTA Range-GET and REST API use cases on constrained devices.
 */

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HTTP_REQUEST_OK           =  0,
    HTTP_REQUEST_ERR_PARAM    = -1,
    HTTP_REQUEST_ERR_OVERFLOW = -2,
} http_request_err_t;

typedef enum {
    HTTP_METHOD_GET  = 0,
    HTTP_METHOD_POST = 1,
    HTTP_METHOD_PUT  = 2,
    HTTP_METHOD_HEAD = 3,
} http_method_t;

typedef struct {
    const char *name;
    const char *value;
} http_header_t;

typedef struct {
    http_method_t method;
    const char   *host;
    const char   *path;
    const http_header_t *headers;
    size_t        header_count;
    const uint8_t *body;
    size_t        body_len;
} http_request_cfg_t;

/**
 * Build a complete HTTP/1.1 request into buf.
 *
 * @param buf      Output buffer.
 * @param cap      Buffer capacity.
 * @param cfg      Request configuration. Must not be NULL.
 * @param out_len  Receives bytes written (excluding NUL). Must not be NULL.
 * @return         HTTP_REQUEST_OK on success.
 * @thread_safe: yes @isr_safe: yes
 */
http_request_err_t http_request_build(uint8_t *buf, size_t cap,
                                      const http_request_cfg_t *cfg,
                                      size_t *out_len);

/**
 * Build a GET request with Range header for byte-range downloads.
 *
 * Convenience wrapper: builds "GET path HTTP/1.1" with Host and
 * "Range: bytes=offset-" (or "Range: bytes=offset-end" if end > 0).
 *
 * @param buf      Output buffer.
 * @param cap      Buffer capacity.
 * @param host     Host header value. Must not be NULL.
 * @param path     Request path. Must not be NULL.
 * @param offset   Byte offset to resume from.
 * @param end      Last byte position (0 = open-ended, i.e. "offset-").
 * @param out_len  Receives bytes written. Must not be NULL.
 * @return         HTTP_REQUEST_OK on success.
 * @thread_safe: yes @isr_safe: yes
 */
http_request_err_t http_request_build_range_get(uint8_t *buf, size_t cap,
                                                const char *host,
                                                const char *path,
                                                size_t offset, size_t end,
                                                size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_REQUEST_H */
