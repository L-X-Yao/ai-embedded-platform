// SPDX-License-Identifier: MIT
/*
 * ibus — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ibus.h"
#include <string.h>

ibus_err_t ibus_parse(const uint8_t *data, size_t len,
                               ibus_msg_t *out)
{
    if (!data || !out) return IBUS_ERR_PARAM;
    if (len < 2) return IBUS_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return IBUS_OK;
}

int ibus_build(uint8_t *buf, size_t cap,
                        const ibus_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
