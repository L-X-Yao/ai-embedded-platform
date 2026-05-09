// SPDX-License-Identifier: MIT
/*
 * dhcp_packet — TODO: one-line description
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "dhcp_packet.h"
#include <string.h>

dhcp_packet_err_t dhcp_packet_parse(const uint8_t *data, size_t len,
                               dhcp_packet_msg_t *out)
{
    if (!data || !out) return DHCP_PACKET_ERR_PARAM;
    if (len < 2) return DHCP_PACKET_ERR_SHORT;

    /* TODO: implement parsing */
    out->type = data[0];
    out->length = (uint16_t)len;

    return DHCP_PACKET_OK;
}

int dhcp_packet_build(uint8_t *buf, size_t cap,
                        const dhcp_packet_msg_t *msg)
{
    if (!buf || !msg) return -1;
    /* TODO: implement building */
    if (cap < 2) return -1;
    buf[0] = msg->type;
    buf[1] = 0; /* placeholder */
    return 2;
}
