// SPDX-License-Identifier: MIT
/*
 * dns_packet — DNS query/response encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "dns_packet.h"
#include "contract.h"
#include <string.h>

/* ---------- helpers: big-endian wire encoding ---------- */

static void put_u16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFF);
}

static uint16_t get_u16(const uint8_t *buf)
{
    return (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
}

static uint32_t get_u32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[2] << 8)  | (uint32_t)buf[3];
}

/* ---------- domain name encoding ---------- */

int dns_encode_name(const char *name, uint8_t *buf, uint16_t buf_size)
{
    if (!name || !buf) return DNS_ERR_PARAM;

    size_t nlen = strlen(name);
    if (nlen == 0 || nlen > 253) return DNS_ERR_FORMAT;

    /* strip trailing dot if present */
    if (name[nlen - 1] == '.') nlen--;
    if (nlen == 0) return DNS_ERR_FORMAT;

    uint16_t pos = 0;
    size_t i = 0;

    while (i < nlen)
    {
        /* find next dot or end */
        size_t label_start = i;
        while (i < nlen && name[i] != '.') i++;
        size_t label_len = i - label_start;

        if (label_len == 0 || label_len > 63) return DNS_ERR_FORMAT;

        /* need: 1 (length byte) + label_len + at least 1 (terminator) */
        if (pos + 1 + (uint16_t)label_len >= buf_size) return DNS_ERR_SIZE;

        buf[pos++] = (uint8_t)label_len;
        memcpy(buf + pos, name + label_start, label_len);
        pos += (uint16_t)label_len;

        if (i < nlen) i++; /* skip dot */
    }

    /* null terminator label */
    if (pos >= buf_size) return DNS_ERR_SIZE;
    buf[pos++] = 0;

    return (int)pos;
}

/* ---------- domain name decoding (with pointer compression) ---------- */

int dns_decode_name(const uint8_t *pkt, uint16_t pkt_len,
                    uint16_t offset, char *name, uint16_t name_size)
{
    if (!pkt || !name || name_size == 0) return DNS_ERR_PARAM;

    uint16_t pos = offset;
    uint16_t out = 0;
    int bytes_consumed = -1;   /* set on first pointer */
    int jumps = 0;
    bool root_reached = false;

    while (pos < pkt_len)
    {
        uint8_t label_len = pkt[pos];

        if (label_len == 0)
        {
            /* end of name */
            if (bytes_consumed < 0)
                bytes_consumed = (int)(pos + 1 - offset);
            root_reached = true;
            break;
        }

        /* pointer compression: top 2 bits = 11 */
        if ((label_len & 0xC0) == 0xC0)
        {
            if (pos + 1 >= pkt_len) return DNS_ERR_FORMAT;

            if (bytes_consumed < 0)
                bytes_consumed = (int)(pos + 2 - offset);

            uint16_t ptr = (uint16_t)(((uint16_t)(label_len & 0x3F) << 8)
                                      | pkt[pos + 1]);
            if (ptr >= pkt_len) return DNS_ERR_FORMAT;

            pos = ptr;
            jumps++;
            if (jumps > 64) return DNS_ERR_FORMAT;  /* infinite loop guard */
            continue;
        }

        /* normal label */
        if (label_len > 63) return DNS_ERR_FORMAT;
        pos++;

        if (pos + label_len > pkt_len) return DNS_ERR_FORMAT;

        /* prepend dot separator (except for first label) */
        if (out > 0)
        {
            if (out >= name_size - 1) return DNS_ERR_OVERFLOW;
            name[out++] = '.';
        }

        if (out + label_len >= name_size) return DNS_ERR_OVERFLOW;
        memcpy(name + out, pkt + pos, label_len);
        out += label_len;
        pos += label_len;
    }

    if (!root_reached) return DNS_ERR_FORMAT;  /* ran off end */

    name[out] = '\0';
    return bytes_consumed;
}

/* ---------- query building ---------- */

int dns_build_query(uint8_t *buf, uint16_t buf_size,
                    uint16_t id, const char *name, dns_type_t type)
{
    if (!buf || !name) return DNS_ERR_PARAM;
    if (buf_size < DNS_HEADER_SIZE) return DNS_ERR_SIZE;

    /* encode name into temp area to measure length */
    uint8_t name_wire[256];
    int name_len = dns_encode_name(name, name_wire, (uint16_t)sizeof(name_wire));
    if (name_len < 0) return name_len;

    /* total = header(12) + name + qtype(2) + qclass(2) */
    uint16_t total = (uint16_t)(DNS_HEADER_SIZE + name_len + 4);
    if (total > buf_size) return DNS_ERR_SIZE;

    /* header: standard query, recursion desired, 1 question */
    memset(buf, 0, DNS_HEADER_SIZE);
    put_u16(buf + 0, id);          /* ID */
    put_u16(buf + 2, 0x0100);      /* flags: QR=0, RD=1 */
    put_u16(buf + 4, 1);           /* QDCOUNT = 1 */
    /* ANCOUNT, NSCOUNT, ARCOUNT = 0 (already zeroed) */

    /* question section */
    uint16_t pos = DNS_HEADER_SIZE;
    memcpy(buf + pos, name_wire, (size_t)name_len);
    pos += (uint16_t)name_len;

    put_u16(buf + pos, (uint16_t)type);
    pos += 2;
    put_u16(buf + pos, (uint16_t)DNS_CLASS_IN);
    pos += 2;

    ENSURE((int)pos <= (int)buf_size);
    return (int)pos;
}

/* ---------- header parsing ---------- */

dns_err_t dns_parse_header(const uint8_t *buf, uint16_t len,
                           dns_header_t *hdr)
{
    if (!buf || !hdr) return DNS_ERR_PARAM;
    if (len < DNS_HEADER_SIZE) return DNS_ERR_SIZE;

    hdr->id          = get_u16(buf + 0);
    hdr->is_response = (buf[2] & 0x80) != 0;
    hdr->rcode       = buf[3] & 0x0F;
    hdr->qdcount     = get_u16(buf + 4);
    hdr->ancount     = get_u16(buf + 6);

    return DNS_OK;
}

/* ---------- record parsing ---------- */

int dns_parse_record(const uint8_t *buf, uint16_t len, uint16_t offset,
                     dns_record_t *record, bool is_answer)
{
    if (!buf || !record) return DNS_ERR_PARAM;
    if (offset >= len) return DNS_ERR_FORMAT;

    memset(record, 0, sizeof(*record));

    /* decode name */
    int name_bytes = dns_decode_name(buf, len, offset,
                                     record->name, DNS_NAME_MAX);
    if (name_bytes < 0) return name_bytes;

    uint16_t pos = offset + (uint16_t)name_bytes;

    /* QTYPE + QCLASS = 4 bytes minimum */
    if (pos + 4 > len) return DNS_ERR_FORMAT;

    record->type = (dns_type_t)get_u16(buf + pos);
    pos += 2;
    record->cls = (dns_class_t)get_u16(buf + pos);
    pos += 2;

    if (is_answer)
    {
        /* TTL(4) + RDLENGTH(2) = 6 bytes */
        if (pos + 6 > len) return DNS_ERR_FORMAT;

        record->ttl = get_u32(buf + pos);
        pos += 4;
        record->rdlength = get_u16(buf + pos);
        pos += 2;

        if (pos + record->rdlength > len) return DNS_ERR_FORMAT;

        record->rdata = buf + pos;
        pos += record->rdlength;
    }

    return (int)pos;
}

/* ---------- utility: extract IPv4 from A record ---------- */

dns_err_t dns_get_ipv4(const dns_record_t *record, uint8_t ip[4])
{
    if (!record || !ip) return DNS_ERR_PARAM;
    if (record->type != DNS_TYPE_A) return DNS_ERR_FORMAT;
    if (record->rdlength != 4 || !record->rdata) return DNS_ERR_FORMAT;

    memcpy(ip, record->rdata, 4);
    return DNS_OK;
}
