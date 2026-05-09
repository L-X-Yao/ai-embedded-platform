// SPDX-License-Identifier: MIT
/*
 * tftp — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "tftp.h"
#include <string.h>

tftp_err_t tftp_parse(const uint8_t *data, size_t len,
                               tftp_msg_t *out)
{
    if (!data || !out) return TFTP_ERR_PARAM;
    if (len < 2) return TFTP_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return TFTP_OK;
}

int tftp_build(uint8_t *buf, size_t cap,
                        const tftp_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
