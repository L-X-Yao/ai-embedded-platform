// SPDX-License-Identifier: MIT
/*
 * dmx512 — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "dmx512.h"
#include <string.h>

dmx512_err_t dmx512_parse(const uint8_t *data, size_t len,
                               dmx512_msg_t *out)
{
    if (!data || !out) return DMX512_ERR_PARAM;
    if (len < 2) return DMX512_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return DMX512_OK;
}

int dmx512_build(uint8_t *buf, size_t cap,
                        const dmx512_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
