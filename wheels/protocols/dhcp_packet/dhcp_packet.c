// SPDX-License-Identifier: MIT
/*
 * dhcp_packet — DHCP message builder/parser (RFC 2131)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "dhcp_packet.h"
#include "contract.h"
#include <string.h>

/* ---------- wire helpers (network byte order = big-endian) ---------- */

static void put_u16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFFU);
}

static uint16_t get_u16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

static void put_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val & 0xFFU);
}

static uint32_t get_u32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[2] << 8)  | (uint32_t)buf[3];
}

/* ---------- fixed-header field offsets (RFC 2131 section 2) ---------- */

#define OFF_OP      0
#define OFF_HTYPE   1
#define OFF_HLEN    2
#define OFF_HOPS    3
#define OFF_XID     4
#define OFF_SECS    8
#define OFF_FLAGS   10
#define OFF_CIADDR  12
#define OFF_YIADDR  16
#define OFF_SIADDR  20
#define OFF_GIADDR  24
#define OFF_CHADDR  28
#define OFF_SNAME   44    /* 64 bytes, unused here */
#define OFF_FILE    108   /* 128 bytes, unused here */
#define OFF_COOKIE  236   /* magic cookie */
#define OFF_OPTIONS 240   /* options start */

/* ---------- option iteration ---------- */

int dhcp_packet_next_option(const uint8_t *buf, size_t len,
                            size_t *offset, dhcp_option_t *opt)
{
    if (!buf || !offset || !opt) return -1;

    while (*offset < len)
    {
        uint8_t code = buf[*offset];

        /* PAD option: single byte, skip */
        if (code == DHCP_OPT_PAD)
        {
            (*offset)++;
            continue;
        }

        /* END option: we are done */
        if (code == DHCP_OPT_END)
        {
            return 0;
        }

        /* TLV: need at least code + len bytes */
        if (*offset + 1 >= len) return -1;

        uint8_t olen = buf[*offset + 1];

        /* data must fit in remaining buffer */
        if (*offset + 2 + olen > len) return -1;

        opt->code = code;
        opt->len  = olen;
        opt->data = buf + *offset + 2;

        *offset = *offset + 2 + olen;
        return 1;
    }

    return 0; /* ran past buffer without END */
}

/* ---------- get specific option ---------- */

int dhcp_packet_get_option(const uint8_t *buf, size_t len,
                           uint8_t code, dhcp_option_t *opt)
{
    if (!buf || !opt) return -1;
    if (len < DHCP_PACKET_MIN_LEN + 4) return -1;

    /* verify magic cookie */
    if (get_u32(buf + OFF_COOKIE) != DHCP_MAGIC_COOKIE) return -1;

    size_t offset = OFF_OPTIONS;
    dhcp_option_t cur;
    int rc;

    while ((rc = dhcp_packet_next_option(buf, len, &offset, &cur)) == 1)
    {
        if (cur.code == code)
        {
            *opt = cur;
            return 0;
        }
    }

    return -1; /* not found */
}

/* ---------- header parsing ---------- */

int dhcp_packet_parse(const uint8_t *buf, size_t len, dhcp_header_t *hdr)
{
    if (!buf || !hdr) return DHCP_PACKET_ERR_PARAM;
    if (len < DHCP_PACKET_MIN_LEN) return DHCP_PACKET_ERR_SHORT;

    /* verify magic cookie at offset 236 */
    if (len >= OFF_COOKIE + 4)
    {
        if (get_u32(buf + OFF_COOKIE) != DHCP_MAGIC_COOKIE)
            return DHCP_PACKET_ERR_FORMAT;
    }

    memset(hdr, 0, sizeof(*hdr));

    hdr->op    = buf[OFF_OP];
    hdr->htype = buf[OFF_HTYPE];
    hdr->hlen  = buf[OFF_HLEN];
    hdr->hops  = buf[OFF_HOPS];
    hdr->xid   = get_u32(buf + OFF_XID);
    hdr->secs  = get_u16(buf + OFF_SECS);
    hdr->flags = get_u16(buf + OFF_FLAGS);

    memcpy(hdr->ciaddr, buf + OFF_CIADDR, 4);
    memcpy(hdr->yiaddr, buf + OFF_YIADDR, 4);
    memcpy(hdr->siaddr, buf + OFF_SIADDR, 4);
    memcpy(hdr->giaddr, buf + OFF_GIADDR, 4);
    memcpy(hdr->chaddr, buf + OFF_CHADDR, 16);

    /* extract msg_type from options if present */
    hdr->msg_type = 0;
    if (len >= OFF_OPTIONS + 3)
    {
        dhcp_option_t opt;
        if (dhcp_packet_get_option(buf, len, DHCP_OPT_MSG_TYPE, &opt) == 0
            && opt.len >= 1)
        {
            hdr->msg_type = opt.data[0];
        }
    }

    return DHCP_PACKET_OK;
}

/* ---------- common base for DISCOVER/REQUEST ---------- */

static int build_base(uint8_t *buf, size_t cap, uint32_t xid,
                      const uint8_t mac[6])
{
    if (!buf || !mac) return DHCP_PACKET_ERR_PARAM;
    if (cap < DHCP_PACKET_BOOTP_MIN_LEN) return DHCP_PACKET_ERR_OVERFLOW;

    memset(buf, 0, DHCP_PACKET_MIN_LEN);

    buf[OFF_OP]    = DHCP_OP_REQUEST;
    buf[OFF_HTYPE] = 1;  /* Ethernet */
    buf[OFF_HLEN]  = 6;

    put_u32(buf + OFF_XID, xid);

    /* flags: broadcast bit set so server sends broadcast reply */
    put_u16(buf + OFF_FLAGS, 0x8000U);

    /* client hardware address (first 6 bytes of chaddr) */
    memcpy(buf + OFF_CHADDR, mac, 6);

    /* magic cookie */
    put_u32(buf + OFF_COOKIE, DHCP_MAGIC_COOKIE);

    return DHCP_PACKET_OK;
}

/* RFC 1542 section 2.1: the UDP length must hold the 300-octet BOOTP header. */
static size_t pad_to_bootp_min(uint8_t *buf, size_t pos)
{
    if (pos >= DHCP_PACKET_BOOTP_MIN_LEN) return pos;
    memset(buf + pos, 0, DHCP_PACKET_BOOTP_MIN_LEN - pos);
    return DHCP_PACKET_BOOTP_MIN_LEN;
}

/* ---------- build DISCOVER ---------- */

int dhcp_packet_build_discover(uint8_t *buf, size_t cap,
                               uint32_t xid, const uint8_t mac[6])
{
    int rc = build_base(buf, cap, xid, mac);
    if (rc != DHCP_PACKET_OK) return rc;

    size_t pos = OFF_OPTIONS;

    /* Option 53: DHCP Message Type = DISCOVER */
    if (pos + 3 > cap) return DHCP_PACKET_ERR_OVERFLOW;
    buf[pos++] = DHCP_OPT_MSG_TYPE;
    buf[pos++] = 1;
    buf[pos++] = DHCP_MSG_DISCOVER;

    /* Option 55: Parameter Request List */
    if (pos + 6 > cap) return DHCP_PACKET_ERR_OVERFLOW;
    buf[pos++] = DHCP_OPT_PARAM_LIST;
    buf[pos++] = 4;
    buf[pos++] = DHCP_OPT_SUBNET_MASK;
    buf[pos++] = DHCP_OPT_ROUTER;
    buf[pos++] = DHCP_OPT_DNS;
    buf[pos++] = DHCP_OPT_LEASE_TIME;

    /* END */
    if (pos + 1 > cap) return DHCP_PACKET_ERR_OVERFLOW;
    buf[pos++] = DHCP_OPT_END;

    pos = pad_to_bootp_min(buf, pos);
    ENSURE(pos <= cap);
    return (int)pos;
}

/* ---------- build REQUEST ---------- */

int dhcp_packet_build_request(uint8_t *buf, size_t cap,
                              uint32_t xid, const uint8_t mac[6],
                              const uint8_t req_ip[4],
                              const uint8_t server_ip[4])
{
    if (!req_ip || !server_ip) return DHCP_PACKET_ERR_PARAM;

    int rc = build_base(buf, cap, xid, mac);
    if (rc != DHCP_PACKET_OK) return rc;

    size_t pos = OFF_OPTIONS;

    /* Option 53: DHCP Message Type = REQUEST */
    if (pos + 3 > cap) return DHCP_PACKET_ERR_OVERFLOW;
    buf[pos++] = DHCP_OPT_MSG_TYPE;
    buf[pos++] = 1;
    buf[pos++] = DHCP_MSG_REQUEST;

    /* Option 50: Requested IP Address */
    if (pos + 6 > cap) return DHCP_PACKET_ERR_OVERFLOW;
    buf[pos++] = DHCP_OPT_REQ_IP;
    buf[pos++] = 4;
    memcpy(buf + pos, req_ip, 4);
    pos += 4;

    /* Option 54: Server Identifier */
    if (pos + 6 > cap) return DHCP_PACKET_ERR_OVERFLOW;
    buf[pos++] = DHCP_OPT_SERVER_ID;
    buf[pos++] = 4;
    memcpy(buf + pos, server_ip, 4);
    pos += 4;

    /* Option 55: Parameter Request List */
    if (pos + 6 > cap) return DHCP_PACKET_ERR_OVERFLOW;
    buf[pos++] = DHCP_OPT_PARAM_LIST;
    buf[pos++] = 4;
    buf[pos++] = DHCP_OPT_SUBNET_MASK;
    buf[pos++] = DHCP_OPT_ROUTER;
    buf[pos++] = DHCP_OPT_DNS;
    buf[pos++] = DHCP_OPT_LEASE_TIME;

    /* END */
    if (pos + 1 > cap) return DHCP_PACKET_ERR_OVERFLOW;
    buf[pos++] = DHCP_OPT_END;

    pos = pad_to_bootp_min(buf, pos);
    ENSURE(pos <= cap);
    return (int)pos;
}

/* ---------- option helpers ---------- */

uint32_t dhcp_packet_option_u32(const dhcp_option_t *opt)
{
    if (!opt || !opt->data || opt->len < 4) return 0;
    return get_u32(opt->data);
}

void dhcp_packet_option_ip(const dhcp_option_t *opt, uint8_t ip[4])
{
    if (!opt || !opt->data || !ip || opt->len < 4)
    {
        if (ip) memset(ip, 0, 4);
        return;
    }
    memcpy(ip, opt->data, 4);
}
