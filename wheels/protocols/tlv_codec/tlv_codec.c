// SPDX-License-Identifier: MIT
/*
 * tlv_codec — Tag-Length-Value encoder/decoder for embedded protocols
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "tlv_codec.h"
#include <string.h>

/* TLV header: 2 bytes tag + 2 bytes length */
#define TLV_HEADER_SIZE  4u

/* ── Encoder ─────────────────────────────────────────────────────── */

void tlv_encoder_init(tlv_encoder_t *enc, uint8_t *buf, uint16_t capacity)
{
    if (!enc) return;
    enc->buf      = buf;
    enc->capacity = (buf != NULL) ? capacity : 0u;
    enc->pos      = 0;
}

tlv_err_t tlv_encode(tlv_encoder_t *enc, uint16_t tag,
                     const uint8_t *value, uint16_t length)
{
    if (!enc || !enc->buf)               return TLV_ERR_PARAM;
    if (length > 0u && value == NULL)    return TLV_ERR_PARAM;

    uint32_t need = (uint32_t)TLV_HEADER_SIZE + (uint32_t)length;
    if ((uint32_t)enc->pos + need > (uint32_t)enc->capacity)
        return TLV_ERR_OVERFLOW;

    uint8_t *p = &enc->buf[enc->pos];

    /* tag — big-endian */
    p[0] = (uint8_t)(tag >> 8);
    p[1] = (uint8_t)(tag & 0xFFu);

    /* length — big-endian */
    p[2] = (uint8_t)(length >> 8);
    p[3] = (uint8_t)(length & 0xFFu);

    /* value */
    if (length > 0u) {
        memcpy(&p[TLV_HEADER_SIZE], value, length);
    }

    enc->pos = (uint16_t)(enc->pos + (uint16_t)need);
    return TLV_OK;
}

tlv_err_t tlv_encode_u8(tlv_encoder_t *enc, uint16_t tag, uint8_t value)
{
    return tlv_encode(enc, tag, &value, 1);
}

tlv_err_t tlv_encode_u16(tlv_encoder_t *enc, uint16_t tag, uint16_t value)
{
    uint8_t tmp[2];
    tmp[0] = (uint8_t)(value >> 8);
    tmp[1] = (uint8_t)(value & 0xFFu);
    return tlv_encode(enc, tag, tmp, 2);
}

tlv_err_t tlv_encode_u32(tlv_encoder_t *enc, uint16_t tag, uint32_t value)
{
    uint8_t tmp[4];
    tmp[0] = (uint8_t)(value >> 24);
    tmp[1] = (uint8_t)((value >> 16) & 0xFFu);
    tmp[2] = (uint8_t)((value >> 8)  & 0xFFu);
    tmp[3] = (uint8_t)(value & 0xFFu);
    return tlv_encode(enc, tag, tmp, 4);
}

tlv_err_t tlv_encode_string(tlv_encoder_t *enc, uint16_t tag, const char *str)
{
    if (!str) return TLV_ERR_PARAM;

    size_t slen = strlen(str);
    if (slen > UINT16_MAX) return TLV_ERR_OVERFLOW;

    return tlv_encode(enc, tag, (const uint8_t *)str, (uint16_t)slen);
}

uint16_t tlv_encoder_len(const tlv_encoder_t *enc)
{
    return enc ? enc->pos : 0;
}

/* ── Decoder ─────────────────────────────────────────────────────── */

void tlv_decoder_init(tlv_decoder_t *dec, const uint8_t *buf, uint16_t len)
{
    if (!dec) return;
    dec->buf = buf;
    dec->len = (buf != NULL) ? len : 0u;
    dec->pos = 0;
}

tlv_err_t tlv_decode_next(tlv_decoder_t *dec, tlv_t *tlv)
{
    if (!dec || !tlv)      return TLV_ERR_PARAM;
    if (!dec->buf)         return TLV_ERR_PARAM;

    /* need at least a full header */
    if ((uint32_t)dec->pos + TLV_HEADER_SIZE > (uint32_t)dec->len)
        return TLV_ERR_TRUNCATED;

    const uint8_t *p = &dec->buf[dec->pos];

    uint16_t tag    = (uint16_t)((uint16_t)p[0] << 8 | p[1]);
    uint16_t length = (uint16_t)((uint16_t)p[2] << 8 | p[3]);

    /* check value fits in remaining buffer */
    if ((uint32_t)dec->pos + TLV_HEADER_SIZE + (uint32_t)length > (uint32_t)dec->len)
        return TLV_ERR_TRUNCATED;

    tlv->tag    = tag;
    tlv->length = length;
    tlv->value  = (length > 0u) ? &p[TLV_HEADER_SIZE] : NULL;

    dec->pos = (uint16_t)(dec->pos + TLV_HEADER_SIZE + length);
    return TLV_OK;
}

bool tlv_decoder_done(const tlv_decoder_t *dec)
{
    if (!dec) return true;
    return dec->pos >= dec->len;
}

/* ── Value extraction helpers ────────────────────────────────────── */

uint8_t tlv_get_u8(const tlv_t *tlv)
{
    if (!tlv || !tlv->value || tlv->length < 1) return 0;
    return tlv->value[0];
}

uint16_t tlv_get_u16(const tlv_t *tlv)
{
    if (!tlv || !tlv->value || tlv->length < 2) return 0;
    return (uint16_t)((uint16_t)tlv->value[0] << 8 | tlv->value[1]);
}

uint32_t tlv_get_u32(const tlv_t *tlv)
{
    if (!tlv || !tlv->value || tlv->length < 4) return 0;
    return (uint32_t)tlv->value[0] << 24
         | (uint32_t)tlv->value[1] << 16
         | (uint32_t)tlv->value[2] << 8
         | (uint32_t)tlv->value[3];
}
