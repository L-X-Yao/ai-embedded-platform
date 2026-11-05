// SPDX-License-Identifier: MIT
/*
 * tftp — TFTP protocol codec (RFC 1350)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "tftp.h"
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

/* ---------- internal: find NUL in buffer ---------- */

static const uint8_t *find_nul(const uint8_t *buf, size_t start, size_t len)
{
    for (size_t i = start; i < len; i++)
    {
        if (buf[i] == 0) return buf + i;
    }
    return NULL;
}

/* ---------- parse ---------- */

int tftp_parse(const uint8_t *buf, size_t len, tftp_packet_t *pkt)
{
    if (!buf || !pkt) return TFTP_ERR_PARAM;
    if (len < 2) return TFTP_ERR_SHORT;

    memset(pkt, 0, sizeof(*pkt));
    pkt->opcode = get_u16(buf);

    switch (pkt->opcode)
    {
    case TFTP_OP_RRQ:
    case TFTP_OP_WRQ:
    {
        /* format: opcode(2) + filename + NUL + mode + NUL */
        if (len < 4) return TFTP_ERR_FORMAT;  /* need at least 2 NULs */

        const uint8_t *fn_end = find_nul(buf, 2, len);
        if (!fn_end) return TFTP_ERR_FORMAT;

        size_t fn_len = (size_t)(fn_end - (buf + 2));
        if (fn_len == 0) return TFTP_ERR_FORMAT;
        /* A shortened name would be a different file. */
        if (fn_len >= TFTP_MAX_FILENAME) return TFTP_ERR_OVERFLOW;

        memcpy(pkt->u.rq.filename, buf + 2, fn_len);
        pkt->u.rq.filename[fn_len] = '\0';

        /* mode string follows filename NUL */
        size_t mode_start = (size_t)(fn_end - buf) + 1;
        if (mode_start >= len) return TFTP_ERR_FORMAT;

        const uint8_t *mode_end = find_nul(buf, mode_start, len);
        if (!mode_end) return TFTP_ERR_FORMAT;

        size_t mode_len = (size_t)(mode_end - (buf + mode_start));
        if (mode_len >= sizeof(pkt->u.rq.mode))
            mode_len = sizeof(pkt->u.rq.mode) - 1;

        memcpy(pkt->u.rq.mode, buf + mode_start, mode_len);
        pkt->u.rq.mode[mode_len] = '\0';
        break;
    }

    case TFTP_OP_DATA:
    {
        /* format: opcode(2) + block#(2) + data(0..512) */
        if (len < 4) return TFTP_ERR_SHORT;

        pkt->u.data.block = get_u16(buf + 2);
        pkt->u.data.data  = (len > 4) ? (buf + 4) : NULL;
        pkt->u.data.len   = len - 4;
        break;
    }

    case TFTP_OP_ACK:
    {
        /* format: opcode(2) + block#(2) */
        if (len < 4) return TFTP_ERR_SHORT;

        pkt->u.ack.block = get_u16(buf + 2);
        break;
    }

    case TFTP_OP_ERROR:
    {
        /* format: opcode(2) + errorcode(2) + errmsg + NUL */
        if (len < 4) return TFTP_ERR_SHORT;

        pkt->u.error.code = get_u16(buf + 2);

        if (len > 4)
        {
            const uint8_t *msg_end = find_nul(buf, 4, len);
            size_t msg_len;
            if (msg_end)
                msg_len = (size_t)(msg_end - (buf + 4));
            else
                msg_len = len - 4;  /* tolerate missing NUL */

            if (msg_len >= sizeof(pkt->u.error.msg))
                msg_len = sizeof(pkt->u.error.msg) - 1;

            memcpy(pkt->u.error.msg, buf + 4, msg_len);
            pkt->u.error.msg[msg_len] = '\0';
        }
        break;
    }

    default:
        return TFTP_ERR_FORMAT;
    }

    return TFTP_OK;
}

/* ---------- build RRQ / WRQ ---------- */

static int build_rq(uint8_t *buf, size_t cap, uint16_t opcode,
                    const char *filename, const char *mode)
{
    if (!buf || !filename || !mode) return TFTP_ERR_PARAM;

    size_t fn_len = strlen(filename);
    size_t mode_len = strlen(mode);

    if (fn_len == 0) return TFTP_ERR_PARAM;

    /* A shortened name would ask for a different file. */
    if (fn_len >= TFTP_MAX_FILENAME) return TFTP_ERR_OVERFLOW;

    /* total: opcode(2) + filename + NUL + mode + NUL */
    size_t total = 2 + fn_len + 1 + mode_len + 1;
    if (total > cap) return TFTP_ERR_OVERFLOW;

    put_u16(buf, opcode);
    size_t pos = 2;

    memcpy(buf + pos, filename, fn_len);
    pos += fn_len;
    buf[pos++] = 0;

    memcpy(buf + pos, mode, mode_len);
    pos += mode_len;
    buf[pos++] = 0;

    ENSURE(pos <= cap);
    return (int)pos;
}

int tftp_build_rrq(uint8_t *buf, size_t cap,
                   const char *filename, const char *mode)
{
    return build_rq(buf, cap, TFTP_OP_RRQ, filename, mode);
}

int tftp_build_wrq(uint8_t *buf, size_t cap,
                   const char *filename, const char *mode)
{
    return build_rq(buf, cap, TFTP_OP_WRQ, filename, mode);
}

/* ---------- build DATA ---------- */

int tftp_build_data(uint8_t *buf, size_t cap,
                    uint16_t block, const uint8_t *data, size_t len)
{
    if (!buf) return TFTP_ERR_PARAM;
    if (len > 0 && !data) return TFTP_ERR_PARAM;
    if (len > TFTP_BLOCK_SIZE) return TFTP_ERR_OVERFLOW;

    size_t total = 4 + len;
    if (total > cap) return TFTP_ERR_OVERFLOW;

    put_u16(buf, TFTP_OP_DATA);
    put_u16(buf + 2, block);

    if (len > 0)
        memcpy(buf + 4, data, len);

    ENSURE(total <= cap);
    return (int)total;
}

/* ---------- build ACK ---------- */

int tftp_build_ack(uint8_t *buf, size_t cap, uint16_t block)
{
    if (!buf) return TFTP_ERR_PARAM;
    if (cap < 4) return TFTP_ERR_OVERFLOW;

    put_u16(buf, TFTP_OP_ACK);
    put_u16(buf + 2, block);

    return 4;
}

/* ---------- build ERROR ---------- */

int tftp_build_error(uint8_t *buf, size_t cap,
                     uint16_t code, const char *msg)
{
    if (!buf || !msg) return TFTP_ERR_PARAM;

    size_t msg_len = strlen(msg);
    /* total: opcode(2) + errcode(2) + msg + NUL */
    size_t total = 4 + msg_len + 1;
    if (total > cap) return TFTP_ERR_OVERFLOW;

    put_u16(buf, TFTP_OP_ERROR);
    put_u16(buf + 2, code);
    memcpy(buf + 4, msg, msg_len);
    buf[4 + msg_len] = 0;

    ENSURE(total <= cap);
    return (int)total;
}

/* ---------- last block detection ---------- */

bool tftp_is_last_data(const tftp_packet_t *pkt)
{
    if (!pkt) return false;
    if (pkt->opcode != TFTP_OP_DATA) return false;
    return pkt->u.data.len < TFTP_BLOCK_SIZE;
}
