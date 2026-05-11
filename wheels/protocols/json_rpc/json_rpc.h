// SPDX-License-Identifier: MIT
/*
 * json_rpc -- JSON-RPC 2.0 frame builder and validator
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef JSON_RPC_H
#define JSON_RPC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- error codes ---------- */

typedef enum {
    JSON_RPC_OK           =  0,
    JSON_RPC_ERR_PARAM    = -1,
    JSON_RPC_ERR_OVERFLOW = -2,
} json_rpc_err_t;

/* ---------- validation result ---------- */

typedef enum {
    JSON_RPC_TYPE_INVALID  = 0,
    JSON_RPC_TYPE_REQUEST,
    JSON_RPC_TYPE_RESPONSE,
    JSON_RPC_TYPE_ERROR,
} json_rpc_type_t;

/* ---------- standard error codes ---------- */

#define JSON_RPC_ERR_PARSE          (-32700)
#define JSON_RPC_ERR_INVALID_REQ    (-32600)
#define JSON_RPC_ERR_METHOD_NOT_FOUND (-32601)
#define JSON_RPC_ERR_INVALID_PARAMS (-32602)
#define JSON_RPC_ERR_INTERNAL       (-32603)

/* ---------- API ---------- */

/** Build a JSON-RPC 2.0 request string.
 *  params_json may be NULL for no params.
 *  @thread_safe: yes  @isr_safe: no */
json_rpc_err_t json_rpc_build_request(char *buf, size_t cap,
                                       const char *method,
                                       const char *params_json,
                                       int32_t id,
                                       size_t *out_len);

/** Build a JSON-RPC 2.0 success response string.
 *  @thread_safe: yes  @isr_safe: no */
json_rpc_err_t json_rpc_build_response(char *buf, size_t cap,
                                        const char *result_json,
                                        int32_t id,
                                        size_t *out_len);

/** Build a JSON-RPC 2.0 error response string.
 *  @thread_safe: yes  @isr_safe: no */
json_rpc_err_t json_rpc_build_error(char *buf, size_t cap,
                                     int32_t code,
                                     const char *message,
                                     int32_t id,
                                     size_t *out_len);

/** Validate a JSON-RPC string and identify its type.
 *  @thread_safe: yes  @isr_safe: no */
json_rpc_type_t json_rpc_validate(const char *json, size_t len);

/** Extract the method name from a JSON-RPC request string.
 *  Sets *method_start to offset and *method_len to length of the method value
 *  (without quotes).
 *  Returns JSON_RPC_OK on success, JSON_RPC_ERR_PARAM if not found.
 *  @thread_safe: yes  @isr_safe: no */
json_rpc_err_t json_rpc_extract_method(const char *json, size_t len,
                                        size_t *method_start,
                                        size_t *method_len);

/** Extract the integer id from a JSON-RPC string.
 *  Returns JSON_RPC_OK on success, JSON_RPC_ERR_PARAM if not found.
 *  @thread_safe: yes  @isr_safe: no */
json_rpc_err_t json_rpc_extract_id(const char *json, size_t len,
                                    int32_t *id);

#ifdef __cplusplus
}
#endif

#endif /* JSON_RPC_H */
