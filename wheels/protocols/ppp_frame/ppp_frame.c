// SPDX-License-Identifier: MIT
/*
 * ppp_frame — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ppp_frame.h"
#include <string.h>

ppp_frame_err_t ppp_frame_parse(const uint8_t *data, size_t len,
                               ppp_frame_msg_t *out)
{
    if (!data || !out) return PPP_FRAME_ERR_PARAM;
    if (len < 2) return PPP_FRAME_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return PPP_FRAME_OK;
}

int ppp_frame_build(uint8_t *buf, size_t cap,
                        const ppp_frame_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
