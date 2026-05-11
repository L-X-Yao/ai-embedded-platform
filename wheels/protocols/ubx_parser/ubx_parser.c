// SPDX-License-Identifier: MIT
/*
 * ubx_parser -- u-blox UBX binary GNSS protocol parser/builder
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ubx_parser.h"
#include "contract.h"
#include <string.h>

/* ---------- checksum ---------- */

void ubx_parser_checksum(const uint8_t *data, size_t len,
                         uint8_t *ck_a, uint8_t *ck_b)
{
    if (!data || !ck_a || !ck_b) return;

    uint8_t a = 0, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (uint8_t)(a + data[i]);
        b = (uint8_t)(b + a);
    }
    *ck_a = a;
    *ck_b = b;
}

/* ---------- init / reset ---------- */

ubx_parser_err_t ubx_parser_init(ubx_parser_ctx_t *ctx,
                                  uint8_t *buf, size_t buf_size)
{
    if (!ctx || !buf) return UBX_PARSER_ERR_PARAM;
    if (buf_size < UBX_OVERHEAD) return UBX_PARSER_ERR_SHORT;

    ctx->buf         = buf;
    ctx->buf_size    = buf_size;
    ctx->idx         = 0;
    ctx->payload_len = 0;
    ctx->state       = UBX_FEED_IDLE;
    return UBX_PARSER_OK;
}

void ubx_parser_reset(ubx_parser_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->idx         = 0;
    ctx->payload_len = 0;
    ctx->state       = UBX_FEED_IDLE;
}

/* ---------- incremental feed ---------- */

ubx_feed_status_t ubx_parser_feed(ubx_parser_ctx_t *ctx, uint8_t byte)
{
    if (!ctx || !ctx->buf) return UBX_FEED_ERROR;

    switch (ctx->state) {
    case UBX_FEED_IDLE:
        if (byte == UBX_SYNC1) {
            ctx->idx = 0;
            ctx->buf[ctx->idx++] = byte;
            ctx->state = UBX_FEED_SYNCING;
        }
        return ctx->state;

    case UBX_FEED_SYNCING:
        if (byte == UBX_SYNC2) {
            ctx->buf[ctx->idx++] = byte;
            ctx->state = UBX_FEED_HEADER;
        } else if (byte == UBX_SYNC1) {
            /* Re-sync: stay in SYNCING with idx reset to sync1 */
            ctx->idx = 0;
            ctx->buf[ctx->idx++] = byte;
        } else {
            ctx->state = UBX_FEED_IDLE;
            ctx->idx = 0;
        }
        return ctx->state;

    case UBX_FEED_HEADER:
        if (ctx->idx >= ctx->buf_size) {
            ctx->state = UBX_FEED_ERROR;
            return ctx->state;
        }
        ctx->buf[ctx->idx++] = byte;
        if (ctx->idx == UBX_HEADER_LEN) {
            /* Parse payload length (little-endian at offset 4,5) */
            ctx->payload_len = (uint16_t)((uint16_t)ctx->buf[5] << 8) |
                               (uint16_t)ctx->buf[4];
            /* Check if payload + checksum fits in buffer */
            if ((size_t)(UBX_HEADER_LEN + ctx->payload_len + 2) > ctx->buf_size) {
                ctx->state = UBX_FEED_ERROR;
                return ctx->state;
            }
            if (ctx->payload_len == 0) {
                /* No payload; next 2 bytes are checksum */
                ctx->state = UBX_FEED_PAYLOAD;
            } else {
                ctx->state = UBX_FEED_PAYLOAD;
            }
        }
        return ctx->state;

    case UBX_FEED_PAYLOAD: {
        if (ctx->idx >= ctx->buf_size) {
            ctx->state = UBX_FEED_ERROR;
            return ctx->state;
        }
        ctx->buf[ctx->idx++] = byte;
        size_t expected_total = (size_t)(UBX_HEADER_LEN + ctx->payload_len + 2);
        if (ctx->idx == expected_total) {
            /* Verify checksum: over class+id+length+payload (buf[2..2+4+payload_len-1]) */
            uint8_t ck_a, ck_b;
            size_t ck_len = (size_t)(4 + ctx->payload_len); /* class+id+len(2)+payload */
            ubx_parser_checksum(&ctx->buf[2], ck_len, &ck_a, &ck_b);
            if (ck_a == ctx->buf[expected_total - 2] &&
                ck_b == ctx->buf[expected_total - 1]) {
                ctx->state = UBX_FEED_COMPLETE;
            } else {
                ctx->state = UBX_FEED_ERROR;
            }
        }
        return ctx->state;
    }

    case UBX_FEED_COMPLETE:
    case UBX_FEED_ERROR:
        /* Must call reset() before feeding more data */
        return ctx->state;
    }

    return UBX_FEED_ERROR;
}

/* ---------- extract ---------- */

ubx_parser_err_t ubx_parser_get_msg(const ubx_parser_ctx_t *ctx,
                                     ubx_msg_t *out)
{
    if (!ctx || !out) return UBX_PARSER_ERR_PARAM;
    if (ctx->state != UBX_FEED_COMPLETE) return UBX_PARSER_ERR_PARAM;

    out->msg_class   = ctx->buf[2];
    out->msg_id      = ctx->buf[3];
    out->payload_len = ctx->payload_len;
    out->payload     = (ctx->payload_len > 0) ? &ctx->buf[UBX_HEADER_LEN] : NULL;

    ENSURE(out->payload_len == ctx->payload_len);
    return UBX_PARSER_OK;
}

/* ---------- build ---------- */

int ubx_parser_build(uint8_t *buf, size_t cap,
                     uint8_t msg_class, uint8_t msg_id,
                     const uint8_t *payload, uint16_t payload_len)
{
    if (!buf) return -1;
    if (payload_len > 0 && !payload) return -1;

    size_t total = (size_t)(UBX_OVERHEAD + payload_len);
    if (cap < total) return -2;

    buf[0] = UBX_SYNC1;
    buf[1] = UBX_SYNC2;
    buf[2] = msg_class;
    buf[3] = msg_id;
    buf[4] = (uint8_t)(payload_len & 0xFF);
    buf[5] = (uint8_t)(payload_len >> 8);

    if (payload_len > 0) {
        memcpy(&buf[UBX_HEADER_LEN], payload, payload_len);
    }

    uint8_t ck_a, ck_b;
    size_t ck_len = (size_t)(4 + payload_len);
    ubx_parser_checksum(&buf[2], ck_len, &ck_a, &ck_b);
    buf[UBX_HEADER_LEN + payload_len]     = ck_a;
    buf[UBX_HEADER_LEN + payload_len + 1] = ck_b;

    ENSURE(total <= cap);
    return (int)total;
}
