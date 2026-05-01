/*
 * tlv_packet — Type-Length-Value encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Wire format:
 *   Tag:    1 byte if < 0x80, else 2 bytes (first byte has bit7 set, tag = ((b0 & 0x7F) << 8) | b1)
 *   Length: 1 byte if < 0x80, else 2 bytes (first byte has bit7 set, len = ((b0 & 0x7F) << 8) | b1)
 *   Value:  `length` bytes
 */

#include "tlv_packet.h"
#include <string.h>

/* ---- internal helpers ---- */

static uint16_t tag_wire_size(uint16_t tag)
{
    return (tag >= 0x80) ? 2u : 1u;
}

static uint16_t len_wire_size(uint16_t length)
{
    return (length >= 0x80) ? 2u : 1u;
}

static uint16_t write_tag(uint8_t *buf, uint16_t tag)
{
    if (tag < 0x80) {
        buf[0] = (uint8_t)tag;
        return 1;
    }
    buf[0] = (uint8_t)(0x80u | (tag >> 8));
    buf[1] = (uint8_t)(tag & 0xFF);
    return 2;
}

static uint16_t write_len(uint8_t *buf, uint16_t length)
{
    if (length < 0x80) {
        buf[0] = (uint8_t)length;
        return 1;
    }
    buf[0] = (uint8_t)(0x80u | (length >> 8));
    buf[1] = (uint8_t)(length & 0xFF);
    return 2;
}

static tlv_err_t read_tag(const uint8_t *buf, uint16_t remaining, uint16_t *tag, uint16_t *consumed)
{
    if (remaining < 1) return TLV_ERR_TRUNCATED;

    if ((buf[0] & 0x80) == 0) {
        *tag = buf[0];
        *consumed = 1;
        return TLV_OK;
    }
    if (remaining < 2) return TLV_ERR_TRUNCATED;
    *tag = (uint16_t)(((uint16_t)(buf[0] & 0x7F) << 8) | buf[1]);
    *consumed = 2;
    return TLV_OK;
}

static tlv_err_t read_len(const uint8_t *buf, uint16_t remaining, uint16_t *length, uint16_t *consumed)
{
    if (remaining < 1) return TLV_ERR_TRUNCATED;

    if ((buf[0] & 0x80) == 0) {
        *length = buf[0];
        *consumed = 1;
        return TLV_OK;
    }
    if (remaining < 2) return TLV_ERR_TRUNCATED;
    *length = (uint16_t)(((uint16_t)(buf[0] & 0x7F) << 8) | buf[1]);
    *consumed = 2;
    return TLV_OK;
}

/* ---- Encoder ---- */

void tlv_encoder_init(tlv_encoder_t *enc, uint8_t *buf, uint16_t capacity)
{
    if (!enc) return;
    enc->buf = buf;
    enc->capacity = capacity;
    enc->pos = 0;
}

tlv_err_t tlv_encode(tlv_encoder_t *enc, uint16_t tag,
                     const uint8_t *value, uint16_t length)
{
    if (!enc || !enc->buf) return TLV_ERR_PARAMS;
    if (length > 0 && !value) return TLV_ERR_PARAMS;

    uint16_t need = (uint16_t)(tag_wire_size(tag) + len_wire_size(length) + length);
    if ((uint32_t)enc->pos + need > enc->capacity) return TLV_ERR_OVERFLOW;

    enc->pos += write_tag(&enc->buf[enc->pos], tag);
    enc->pos += write_len(&enc->buf[enc->pos], length);

    if (length > 0) {
        memcpy(&enc->buf[enc->pos], value, length);
        enc->pos += length;
    }

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
    tmp[1] = (uint8_t)(value & 0xFF);
    return tlv_encode(enc, tag, tmp, 2);
}

tlv_err_t tlv_encode_u32(tlv_encoder_t *enc, uint16_t tag, uint32_t value)
{
    uint8_t tmp[4];
    tmp[0] = (uint8_t)(value >> 24);
    tmp[1] = (uint8_t)(value >> 16);
    tmp[2] = (uint8_t)(value >> 8);
    tmp[3] = (uint8_t)(value & 0xFF);
    return tlv_encode(enc, tag, tmp, 4);
}

tlv_err_t tlv_encode_string(tlv_encoder_t *enc, uint16_t tag, const char *str)
{
    if (!str) return TLV_ERR_PARAMS;
    size_t slen = strlen(str);
    if (slen > UINT16_MAX) return TLV_ERR_OVERFLOW;
    return tlv_encode(enc, tag, (const uint8_t *)str, (uint16_t)slen);
}

uint16_t tlv_encoder_len(const tlv_encoder_t *enc)
{
    return enc ? enc->pos : 0;
}

/* ---- Decoder ---- */

void tlv_decoder_init(tlv_decoder_t *dec, const uint8_t *buf, uint16_t len)
{
    if (!dec) return;
    dec->buf = buf;
    dec->len = len;
    dec->pos = 0;
}

tlv_err_t tlv_decode_next(tlv_decoder_t *dec, tlv_t *tlv)
{
    if (!dec || !tlv) return TLV_ERR_PARAMS;
    if (!dec->buf) return TLV_ERR_PARAMS;
    if (dec->pos >= dec->len) return TLV_ERR_TRUNCATED;

    uint16_t remaining = (uint16_t)(dec->len - dec->pos);
    uint16_t tag, tag_bytes;
    tlv_err_t err = read_tag(&dec->buf[dec->pos], remaining, &tag, &tag_bytes);
    if (err != TLV_OK) return err;

    remaining = (uint16_t)(remaining - tag_bytes);
    uint16_t length, len_bytes;
    err = read_len(&dec->buf[dec->pos + tag_bytes], remaining, &length, &len_bytes);
    if (err != TLV_OK) return err;

    remaining = (uint16_t)(remaining - len_bytes);
    if (length > remaining) return TLV_ERR_TRUNCATED;

    tlv->tag = tag;
    tlv->length = length;
    tlv->value = (length > 0) ? &dec->buf[dec->pos + tag_bytes + len_bytes] : NULL;

    dec->pos = (uint16_t)(dec->pos + tag_bytes + len_bytes + length);
    return TLV_OK;
}

bool tlv_decoder_done(const tlv_decoder_t *dec)
{
    if (!dec) return true;
    return dec->pos >= dec->len;
}

/* ---- Search ---- */

tlv_err_t tlv_find(const uint8_t *buf, uint16_t len, uint16_t tag, tlv_t *result)
{
    if (!buf || !result) return TLV_ERR_PARAMS;

    tlv_decoder_t dec;
    tlv_decoder_init(&dec, buf, len);

    tlv_t tlv;
    while (!tlv_decoder_done(&dec)) {
        tlv_err_t err = tlv_decode_next(&dec, &tlv);
        if (err != TLV_OK) return err;
        if (tlv.tag == tag) {
            *result = tlv;
            return TLV_OK;
        }
    }
    return TLV_ERR_FORMAT;  /* tag not found */
}
