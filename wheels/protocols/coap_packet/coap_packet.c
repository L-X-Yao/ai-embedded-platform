// SPDX-License-Identifier: MIT
/*
 * coap_packet — CoAP message encoder/decoder (RFC 7252)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "coap_packet.h"
#include "contract.h"
#include <string.h>

coap_packet_err_t coap_packet_parse(const uint8_t *data, size_t len, coap_packet_t *pkt)
{
    if (!data || !pkt)
        return COAP_PACKET_ERR_PARAM;
    if (len < 4)
        return COAP_PACKET_ERR_INCOMPLETE;

    memset(pkt, 0, sizeof(*pkt));

    pkt->version = (data[0] >> 6) & 0x03;
    if (pkt->version != 1)
        return COAP_PACKET_ERR_MALFORMED;

    pkt->type = (coap_packet_type_t)((data[0] >> 4) & 0x03);
    pkt->token_len = data[0] & 0x0F;
    pkt->code = data[1];
    pkt->message_id = (uint16_t)(((uint16_t)data[2] << 8) | data[3]);

    size_t pos = 4;

    if (pkt->token_len > 8)
        return COAP_PACKET_ERR_MALFORMED;
    if (pos + pkt->token_len > len)
        return COAP_PACKET_ERR_INCOMPLETE;
    pkt->token = &data[pos];
    pos += pkt->token_len;

    uint16_t prev_opt_num = 0;
    pkt->option_count = 0;

    while (pos < len && data[pos] != 0xFF) {
        if (pos >= len)
            break;
        uint8_t byte = data[pos++];
        uint16_t delta = (byte >> 4) & 0x0F;
        uint16_t opt_len = byte & 0x0F;

        if (delta == 13) {
            if (pos >= len) return COAP_PACKET_ERR_INCOMPLETE;
            delta = (uint16_t)(data[pos++] + 13);
        } else if (delta == 14) {
            if (pos + 1 >= len) return COAP_PACKET_ERR_INCOMPLETE;
            uint32_t raw_delta = ((uint32_t)data[pos] << 8 | data[pos + 1]) + 269u;
            if (raw_delta > UINT16_MAX) return COAP_PACKET_ERR_MALFORMED;
            delta = (uint16_t)raw_delta;
            pos += 2;
        } else if (delta == 15) {
            return COAP_PACKET_ERR_MALFORMED;
        }

        if (opt_len == 13) {
            if (pos >= len) return COAP_PACKET_ERR_INCOMPLETE;
            opt_len = (uint16_t)(data[pos++] + 13);
        } else if (opt_len == 14) {
            if (pos + 1 >= len) return COAP_PACKET_ERR_INCOMPLETE;
            uint32_t raw_olen = ((uint32_t)data[pos] << 8 | data[pos + 1]) + 269u;
            if (raw_olen > UINT16_MAX) return COAP_PACKET_ERR_MALFORMED;
            opt_len = (uint16_t)raw_olen;
            pos += 2;
        } else if (opt_len == 15) {
            return COAP_PACKET_ERR_MALFORMED;
        }

        if (opt_len > len - pos)
            return COAP_PACKET_ERR_INCOMPLETE;

        uint32_t opt_num32 = (uint32_t)prev_opt_num + delta;
        if (opt_num32 > UINT16_MAX) return COAP_PACKET_ERR_MALFORMED;
        uint16_t opt_num = (uint16_t)opt_num32;

        if (pkt->option_count < COAP_MAX_OPTIONS) {
            pkt->options[pkt->option_count].number = opt_num;
            pkt->options[pkt->option_count].value = &data[pos];
            pkt->options[pkt->option_count].value_len = opt_len;
            pkt->option_count++;
        } else {
            return COAP_PACKET_ERR_TOO_MANY;
        }

        prev_opt_num = opt_num;
        pos += opt_len;
    }

    if (pos < len && data[pos] == 0xFF) {
        pos++;
        if (pos >= len) return COAP_PACKET_ERR_MALFORMED;
        pkt->payload = &data[pos];
        pkt->payload_len = len - pos;
        pos = len;
    }

    pkt->total_len = pos;
    ENSURE(pkt->total_len <= len);
    return COAP_PACKET_OK;
}

static size_t encode_opt_header(uint8_t *buf, uint16_t delta, uint16_t opt_len)
{
    size_t pos = 0;
    uint8_t d, l;

    if (delta < 13) d = (uint8_t)delta;
    else if (delta < 269) d = 13;
    else d = 14;

    if (opt_len < 13) l = (uint8_t)opt_len;
    else if (opt_len < 269) l = 13;
    else l = 14;

    buf[pos++] = (uint8_t)((d << 4) | l);

    if (d == 13) buf[pos++] = (uint8_t)(delta - 13);
    else if (d == 14) {
        uint16_t v = (uint16_t)(delta - 269);
        buf[pos++] = (uint8_t)(v >> 8);
        buf[pos++] = (uint8_t)(v & 0xFF);
    }

    if (l == 13) buf[pos++] = (uint8_t)(opt_len - 13);
    else if (l == 14) {
        uint16_t v = (uint16_t)(opt_len - 269);
        buf[pos++] = (uint8_t)(v >> 8);
        buf[pos++] = (uint8_t)(v & 0xFF);
    }

    return pos;
}

coap_packet_err_t coap_packet_build(uint8_t *buf, size_t buf_len, size_t *out_len,
                      coap_packet_type_t type, uint8_t code, uint16_t message_id,
                      const uint8_t *token, uint8_t token_len,
                      const coap_packet_option_t *options, int option_count,
                      const uint8_t *payload, size_t payload_len)
{
    if (!buf || !out_len)
        return COAP_PACKET_ERR_PARAM;
    if (token_len > 8)
        return COAP_PACKET_ERR_PARAM;
    if (option_count > 0 && !options)
        return COAP_PACKET_ERR_PARAM;

    size_t pos = 0;

    if (buf_len < 4u + token_len)
        return COAP_PACKET_ERR_OVERFLOW;

    buf[pos++] = (uint8_t)((1u << 6) | ((uint8_t)type << 4) | token_len);
    buf[pos++] = code;
    buf[pos++] = (uint8_t)(message_id >> 8);
    buf[pos++] = (uint8_t)(message_id & 0xFF);

    if (token && token_len > 0) {
        memcpy(&buf[pos], token, token_len);
        pos += token_len;
    }

    uint16_t prev_num = 0;
    for (int i = 0; i < option_count; i++) {
        if (options[i].number < prev_num) return COAP_PACKET_ERR_MALFORMED;
        uint16_t delta = (uint16_t)(options[i].number - prev_num);
        if (options[i].value_len > UINT16_MAX) return COAP_PACKET_ERR_OVERFLOW;
        uint8_t hdr[5];
        size_t hdr_len = encode_opt_header(hdr, delta, (uint16_t)options[i].value_len);

        if (pos + hdr_len + options[i].value_len > buf_len)
            return COAP_PACKET_ERR_OVERFLOW;

        memcpy(&buf[pos], hdr, hdr_len);
        pos += hdr_len;
        if (options[i].value && options[i].value_len > 0) {
            memcpy(&buf[pos], options[i].value, options[i].value_len);
            pos += options[i].value_len;
        }
        prev_num = options[i].number;
    }

    if (payload && payload_len > 0) {
        if (pos + 1 + payload_len > buf_len)
            return COAP_PACKET_ERR_OVERFLOW;
        buf[pos++] = 0xFF;
        memcpy(&buf[pos], payload, payload_len);
        pos += payload_len;
    }

    *out_len = pos;
    ENSURE(pos <= buf_len);
    return COAP_PACKET_OK;
}
