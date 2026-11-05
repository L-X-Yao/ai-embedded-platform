// SPDX-License-Identifier: MIT
/*
 * json_rpc -- JSON-RPC 2.0 frame builder and validator
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "json_rpc.h"
#include "contract.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------- internal helpers ---------- */

/**
 * Safe snprintf wrapper. Returns number of characters written (excluding NUL),
 * or -1 if buffer too small.
 */
static int safe_snprintf(char *buf, size_t cap, const char *fmt, ...)
{
    if (!buf || cap == 0) return -1;

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, cap, fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n >= cap) return -1;
    return n;
}

/**
 * Find a JSON key in a string. Returns pointer to the character after
 * the colon following the key, or NULL if not found.
 * Simple string search -- not a full JSON parser.
 */
static const char *find_key(const char *json, size_t len, const char *key)
{
    if (!json || !key) return NULL;

    size_t key_len = strlen(key);
    /* Search for "key" pattern */
    const char *end = json + len;

    for (const char *p = json; p < end; p++) {
        if (*p != '"') continue;
        /* Check if this quoted string matches key */
        if ((size_t)(end - p - 1) < key_len + 1) continue;
        if (memcmp(p + 1, key, key_len) != 0) continue;
        if (p[1 + key_len] != '"') continue;

        /* Found "key" -- now find the colon */
        const char *after_key = p + 1 + key_len + 1;
        while (after_key < end && (*after_key == ' ' || *after_key == '\t'))
            after_key++;
        if (after_key < end && *after_key == ':') {
            after_key++;
            while (after_key < end && (*after_key == ' ' || *after_key == '\t'))
                after_key++;
            return after_key;
        }
    }
    return NULL;
}

/* ---------- build ---------- */

json_rpc_err_t json_rpc_build_request(char *buf, size_t cap,
                                       const char *method,
                                       const char *params_json,
                                       int32_t id,
                                       size_t *out_len)
{
    if (!buf || !method || !out_len) return JSON_RPC_ERR_PARAM;

    int n;
    if (params_json) {
        n = safe_snprintf(buf, cap,
            "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":%d}",
            method, params_json, (int)id);
    } else {
        n = safe_snprintf(buf, cap,
            "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"id\":%d}",
            method, (int)id);
    }

    if (n < 0) return JSON_RPC_ERR_OVERFLOW;
    *out_len = (size_t)n;
    return JSON_RPC_OK;
}

json_rpc_err_t json_rpc_build_response(char *buf, size_t cap,
                                        const char *result_json,
                                        int32_t id,
                                        size_t *out_len)
{
    if (!buf || !result_json || !out_len) return JSON_RPC_ERR_PARAM;

    int n = safe_snprintf(buf, cap,
        "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":%d}",
        result_json, (int)id);

    if (n < 0) return JSON_RPC_ERR_OVERFLOW;
    *out_len = (size_t)n;
    return JSON_RPC_OK;
}

json_rpc_err_t json_rpc_build_error(char *buf, size_t cap,
                                     int32_t code,
                                     const char *message,
                                     int32_t id,
                                     size_t *out_len)
{
    if (!buf || !message || !out_len) return JSON_RPC_ERR_PARAM;

    int n = safe_snprintf(buf, cap,
        "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":\"%s\"},\"id\":%d}",
        (int)code, message, (int)id);

    if (n < 0) return JSON_RPC_ERR_OVERFLOW;
    *out_len = (size_t)n;
    return JSON_RPC_OK;
}

/* ---------- validate ---------- */

json_rpc_type_t json_rpc_validate(const char *json, size_t len)
{
    if (!json || len == 0) return JSON_RPC_TYPE_INVALID;

    /* Must contain "jsonrpc":"2.0" */
    const char *ver = find_key(json, len, "jsonrpc");
    if (!ver) return JSON_RPC_TYPE_INVALID;

    /* Check version value starts with "2.0" */
    const char *end = json + len;
    if (ver >= end || *ver != '"') return JSON_RPC_TYPE_INVALID;
    if ((size_t)(end - ver) < 5) return JSON_RPC_TYPE_INVALID;
    if (memcmp(ver, "\"2.0\"", 5) != 0) return JSON_RPC_TYPE_INVALID;

    /* Classify: error > result > method */
    if (find_key(json, len, "error")) return JSON_RPC_TYPE_ERROR;
    if (find_key(json, len, "result")) return JSON_RPC_TYPE_RESPONSE;
    if (find_key(json, len, "method")) return JSON_RPC_TYPE_REQUEST;

    return JSON_RPC_TYPE_INVALID;
}

/* ---------- extract ---------- */

json_rpc_err_t json_rpc_extract_method(const char *json, size_t len,
                                        size_t *method_start,
                                        size_t *method_len)
{
    if (!json || !method_start || !method_len) return JSON_RPC_ERR_PARAM;

    const char *val = find_key(json, len, "method");
    if (!val) return JSON_RPC_ERR_PARAM;

    const char *end = json + len;
    if (val >= end || *val != '"') return JSON_RPC_ERR_PARAM;

    val++; /* skip opening quote */
    const char *close = val;
    while (close < end && *close != '"') close++;
    if (close >= end) return JSON_RPC_ERR_PARAM;

    *method_start = (size_t)(val - json);
    *method_len   = (size_t)(close - val);
    return JSON_RPC_OK;
}

json_rpc_err_t json_rpc_extract_id(const char *json, size_t len,
                                    int32_t *id)
{
    if (!json || !id) return JSON_RPC_ERR_PARAM;

    const char *val = find_key(json, len, "id");
    if (!val) return JSON_RPC_ERR_PARAM;

    const char *end = json + len;
    if (val >= end) return JSON_RPC_ERR_PARAM;

    /* Parse integer value (possibly negative) */
    int64_t sign = 1;
    if (*val == '-') {
        sign = -1;
        val++;
    }
    if (val >= end || *val < '0' || *val > '9') return JSON_RPC_ERR_PARAM;

    int64_t limit = (sign < 0) ? (int64_t)INT32_MAX + 1 : INT32_MAX;
    int64_t result = 0;
    while (val < end && *val >= '0' && *val <= '9') {
        result = result * 10 + (*val - '0');
        if (result > limit) return JSON_RPC_ERR_OVERFLOW;
        val++;
    }

    *id = (int32_t)(sign * result);
    return JSON_RPC_OK;
}
