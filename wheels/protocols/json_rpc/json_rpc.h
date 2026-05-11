// SPDX-License-Identifier: MIT
/*
 * json_rpc — TODO: one-line description
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
    JSON_RPC_ERR_FORMAT   = -2,
    JSON_RPC_ERR_SHORT    = -3,
    JSON_RPC_ERR_OVERFLOW = -4,
} json_rpc_err_t;

/* ---------- data types ---------- */

typedef struct {
    /* TODO: parsed fields */
    uint8_t type;
    uint16_t length;
} json_rpc_msg_t;

/* ---------- API ---------- */

/** Parse a message from raw bytes. @thread_safe: yes @isr_safe: no */
json_rpc_err_t json_rpc_parse(const uint8_t *data, size_t len,
                               json_rpc_msg_t *out);

/** Build a message into a buffer. Returns >0 bytes written, -1 param error, -2 buffer too small. @thread_safe: yes @isr_safe: no */
int json_rpc_build(uint8_t *buf, size_t cap,
                        const json_rpc_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* JSON_RPC_H */
