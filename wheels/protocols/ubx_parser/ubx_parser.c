// SPDX-License-Identifier: MIT
/*
 * ubx_parser — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ubx_parser.h"
#include <string.h>

ubx_parser_err_t ubx_parser_parse(const uint8_t *data, size_t len,
                               ubx_parser_msg_t *out)
{
    if (!data || !out) return UBX_PARSER_ERR_PARAM;
    if (len < 2) return UBX_PARSER_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return UBX_PARSER_OK;
}

int ubx_parser_build(uint8_t *buf, size_t cap,
                        const ubx_parser_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
