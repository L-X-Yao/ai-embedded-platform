// SPDX-License-Identifier: MIT
/*
 * json_rpc — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "json_rpc.h"
#include <string.h>

json_rpc_err_t json_rpc_parse(const uint8_t *data, size_t len,
                               json_rpc_msg_t *out)
{
    if (!data || !out) return JSON_RPC_ERR_PARAM;
    if (len < 2) return JSON_RPC_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return JSON_RPC_OK;
}

int json_rpc_build(uint8_t *buf, size_t cap,
                        const json_rpc_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
