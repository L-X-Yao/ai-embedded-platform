// SPDX-License-Identifier: MIT
/*
 * crsf — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "crsf.h"
#include <string.h>

crsf_err_t crsf_parse(const uint8_t *data, size_t len,
                               crsf_msg_t *out)
{
    if (!data || !out) return CRSF_ERR_PARAM;
    if (len < 2) return CRSF_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return CRSF_OK;
}

int crsf_build(uint8_t *buf, size_t cap,
                        const crsf_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
