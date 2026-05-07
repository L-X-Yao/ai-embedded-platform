// SPDX-License-Identifier: MIT
/*
 * mavlink -- MAVLink v2 frame parser and serializer
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "mavlink.h"
#include "contract.h"
#include <string.h>

/* ---------- CRC-16/MCRF4XX (X.25) ---------- */

static uint16_t crc_accumulate(uint8_t byte, uint16_t crc)
{
    uint8_t tmp;
    tmp = byte ^ (uint8_t)(crc & 0xFF);
    tmp ^= (uint8_t)(tmp << 4);
    crc = (uint16_t)((crc >> 8) ^ ((uint16_t)tmp << 8)
                     ^ ((uint16_t)tmp << 3)
                     ^ ((uint16_t)tmp >> 4));
    return crc;
}

uint16_t mavlink_crc16(const uint8_t *data, uint16_t len, uint16_t init)
{
    if (!data) return init;
    uint16_t crc = init;
    for (uint16_t i = 0; i < len; i++) {
        crc = crc_accumulate(data[i], crc);
    }
    return crc;
}

/* ---------- CRC_EXTRA lookup ---------- */

uint8_t mavlink_msg_crc_extra(uint32_t msgid)
{
    switch (msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT:           return 50;
    case MAVLINK_MSG_ID_SYS_STATUS:          return 124;
    case MAVLINK_MSG_ID_GPS_RAW_INT:         return 24;
    case MAVLINK_MSG_ID_ATTITUDE:            return 39;
    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: return 104;
    default:                                 return 0;
    }
}

/* ---------- parser ---------- */

/*
 * MAVLink v2 frame layout (no signature):
 *
 *  Byte  Field
 *  ----  -----
 *   0    STX (0xFD)
 *   1    LEN (payload length)
 *   2    incompat_flags
 *   3    compat_flags
 *   4    seq
 *   5    sysid
 *   6    compid
 *   7    msgid[0] (low)
 *   8    msgid[1]
 *   9    msgid[2] (high)
 *  10..  payload[0..LEN-1]
 *  10+LEN  checksum low
 *  11+LEN  checksum high
 *
 * CRC covers bytes 1..9+LEN (everything after STX, before checksum),
 * then one extra byte: the CRC_EXTRA for the message type.
 *
 * rx_count tracks bytes received AFTER the STX.
 * rx_count=0 means we just got STX and are waiting for LEN (byte 1).
 */

mavlink_err_t mavlink_parser_init(mavlink_parser_t *parser)
{
    if (!parser) return MAVLINK_ERR_PARAM;
    memset(parser, 0, sizeof(*parser));
    parser->state = MAVLINK_PARSE_IDLE;
    return MAVLINK_OK;
}

mavlink_parse_state_t mavlink_parse_byte(mavlink_parser_t *parser, uint8_t byte)
{
    if (!parser) return MAVLINK_PARSE_ERROR;

    /* Auto-reset after terminal states */
    if (parser->state == MAVLINK_PARSE_COMPLETE ||
        parser->state == MAVLINK_PARSE_ERROR) {
        parser->state    = MAVLINK_PARSE_IDLE;
        parser->rx_count = 0;
        parser->crc      = 0;
    }

    /* IDLE: wait for STX */
    if (parser->state == MAVLINK_PARSE_IDLE) {
        if (byte == MAVLINK_STX) {
            memset(&parser->msg, 0, sizeof(parser->msg));
            parser->msg.stx  = MAVLINK_STX;
            parser->rx_count = 0;
            parser->crc      = 0xFFFF;  /* CRC init */
            parser->state    = MAVLINK_PARSE_GOT_STX;
        }
        /* non-STX bytes silently dropped */
        return parser->state;
    }

    /* After STX: accumulate header, payload, checksum */
    uint16_t idx = parser->rx_count;  /* 0-based post-STX */

    /* Header fields: bytes 0..8 (rx_count 0..8) */
    if (idx < 9) {
        parser->crc = crc_accumulate(byte, parser->crc);

        switch (idx) {
        case 0: parser->msg.len            = byte;                  break;
        case 1: parser->msg.incompat_flags = byte;                  break;
        case 2: parser->msg.compat_flags   = byte;                  break;
        case 3: parser->msg.seq            = byte;                  break;
        case 4: parser->msg.sysid          = byte;                  break;
        case 5: parser->msg.compid         = byte;                  break;
        case 6: parser->msg.msgid          = (uint32_t)byte;        break;
        case 7: parser->msg.msgid         |= (uint32_t)byte << 8;  break;
        case 8: parser->msg.msgid         |= (uint32_t)byte << 16; break;
        default: break;
        }

        parser->rx_count++;
        parser->state = MAVLINK_PARSE_IN_PROGRESS;

        /* Reject unsupported incompat_flags (bit 0 = signature required) */
        if (idx == 8 && parser->msg.incompat_flags != 0) {
            parser->state = MAVLINK_PARSE_ERROR;
        }

        return parser->state;
    }

    /* Payload: rx_count 9 .. 9+len-1 */
    uint16_t payload_end = (uint16_t)(9u + (uint16_t)parser->msg.len);

    if (idx < payload_end) {
        parser->crc = crc_accumulate(byte, parser->crc);
        parser->msg.payload[idx - 9u] = byte;
        parser->rx_count++;
        parser->state = MAVLINK_PARSE_IN_PROGRESS;
        return parser->state;
    }

    /* Checksum byte 1 (low) */
    if (idx == payload_end) {
        /* Fold in CRC_EXTRA before comparing */
        uint8_t extra = mavlink_msg_crc_extra(parser->msg.msgid);
        parser->crc = crc_accumulate(extra, parser->crc);

        parser->msg.checksum = (uint16_t)byte;
        parser->rx_count++;
        parser->state = MAVLINK_PARSE_IN_PROGRESS;
        return parser->state;
    }

    /* Checksum byte 2 (high) */
    if (idx == (uint16_t)(payload_end + 1u)) {
        parser->msg.checksum |= (uint16_t)((uint16_t)byte << 8);
        parser->rx_count++;

        if (parser->msg.checksum == parser->crc) {
            parser->state = MAVLINK_PARSE_COMPLETE;
        } else {
            parser->state = MAVLINK_PARSE_ERROR;
        }
        return parser->state;
    }

    /* Should not reach here */
    parser->state = MAVLINK_PARSE_ERROR;
    return parser->state;
}

const mavlink_msg_t *mavlink_get_msg(const mavlink_parser_t *parser)
{
    if (!parser) return NULL;
    if (parser->state != MAVLINK_PARSE_COMPLETE) return NULL;
    return &parser->msg;
}

/* ---------- serializer ---------- */

mavlink_err_t mavlink_serialize(const mavlink_msg_t *msg,
                                uint8_t *buf, uint16_t buf_len,
                                uint16_t *out_len, uint8_t crc_extra)
{
    if (!msg || !buf || !out_len) return MAVLINK_ERR_PARAM;

    uint16_t frame_len = (uint16_t)(MAVLINK_HEADER_LEN +
                                    (uint16_t)msg->len +
                                    MAVLINK_CHECKSUM_LEN);
    if (buf_len < frame_len) return MAVLINK_ERR_SIZE;

    /* STX */
    buf[0] = MAVLINK_STX;

    /* Header bytes 1..9 */
    buf[1] = msg->len;
    buf[2] = msg->incompat_flags;
    buf[3] = msg->compat_flags;
    buf[4] = msg->seq;
    buf[5] = msg->sysid;
    buf[6] = msg->compid;
    buf[7] = (uint8_t)(msg->msgid & 0xFF);
    buf[8] = (uint8_t)((msg->msgid >> 8) & 0xFF);
    buf[9] = (uint8_t)((msg->msgid >> 16) & 0xFF);

    /* Payload */
    if (msg->len > 0) {
        memcpy(&buf[MAVLINK_HEADER_LEN], msg->payload, msg->len);
    }

    /* CRC: covers bytes 1..9+payload (everything after STX, before checksum) */
    uint16_t crc_len = (uint16_t)(9u + (uint16_t)msg->len);
    uint16_t crc = mavlink_crc16(&buf[1], crc_len, 0xFFFF);
    crc = crc_accumulate(crc_extra, crc);

    /* Checksum (little-endian) */
    uint16_t ck_off = (uint16_t)(MAVLINK_HEADER_LEN + (uint16_t)msg->len);
    buf[ck_off]     = (uint8_t)(crc & 0xFF);
    buf[ck_off + 1] = (uint8_t)(crc >> 8);

    *out_len = frame_len;
    return MAVLINK_OK;
}
