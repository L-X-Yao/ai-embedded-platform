// SPDX-License-Identifier: MIT
/*
 * protobuf_lite — lightweight Protocol Buffers wire format encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "protobuf_lite.h"
#include "contract.h"
#include <string.h>

/* ---- Internal helpers ---- */

/* Max varint bytes for uint64: ceil(64/7) = 10 */
#define PB_MAX_VARINT_BYTES 10

static uint16_t pb_varint_size(uint64_t v)
{
    uint16_t n = 1;
    while (v > 0x7F) { v >>= 7; n++; }
    return n;
}

static pb_err_t pb_put_varint(pb_encoder_t *enc, uint64_t v)
{
    uint16_t need = pb_varint_size(v);
    if ((uint16_t)(enc->capacity - enc->pos) < need) {
        return PB_ERR_OVERFLOW;
    }
    while (v > 0x7F) {
        enc->buf[enc->pos++] = (uint8_t)(v & 0x7F) | 0x80;
        v >>= 7;
    }
    enc->buf[enc->pos++] = (uint8_t)v;
    return PB_OK;
}

static pb_err_t pb_put_tag(pb_encoder_t *enc, uint8_t field_num,
                           pb_wire_type_t wt)
{
    uint32_t tag = ((uint32_t)field_num << 3) | (uint32_t)wt;
    return pb_put_varint(enc, tag);
}

static pb_err_t pb_get_varint(pb_decoder_t *dec, uint64_t *out)
{
    uint64_t result = 0;
    unsigned shift = 0;

    for (int i = 0; i < PB_MAX_VARINT_BYTES; i++) {
        if (dec->pos >= dec->len) {
            return PB_ERR_TRUNCATED;
        }
        uint8_t b = dec->buf[dec->pos++];
        result |= (uint64_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            *out = result;
            return PB_OK;
        }
        shift += 7;
    }
    return PB_ERR_MALFORMED;
}

/* ---- Encoder ---- */

void pb_encoder_init(pb_encoder_t *enc, uint8_t *buf, uint16_t capacity)
{
    if (!enc) return;
    enc->buf = buf;
    enc->capacity = capacity;
    enc->pos = 0;
    ENSURE(enc->pos == 0);
}

pb_err_t pb_encode_varint(pb_encoder_t *enc, uint8_t field_num, uint64_t value)
{
    if (!enc || !enc->buf || field_num == 0) return PB_ERR_PARAM;
    pb_err_t err = pb_put_tag(enc, field_num, PB_WIRE_VARINT);
    if (err != PB_OK) return err;
    return pb_put_varint(enc, value);
}

pb_err_t pb_encode_sint32(pb_encoder_t *enc, uint8_t field_num, int32_t value)
{
    /* ZigZag encoding: cast to unsigned first to avoid UB on left-shift of
     * negative signed integers (C99 6.5.7p4). Arithmetic right-shift of signed
     * int is implementation-defined but universally sign-extends on all targets
     * we care about; the cast-then-shift approach is fully defined. */
    uint32_t uv = (uint32_t)value;
    uint32_t zz = (uv << 1) ^ (uint32_t)(value >> 31);
    return pb_encode_varint(enc, field_num, zz);
}

pb_err_t pb_encode_sint64(pb_encoder_t *enc, uint8_t field_num, int64_t value)
{
    /* ZigZag: cast to unsigned for left-shift to avoid UB. */
    uint64_t uv = (uint64_t)value;
    uint64_t zz = (uv << 1) ^ (uint64_t)(value >> 63);
    return pb_encode_varint(enc, field_num, zz);
}

pb_err_t pb_encode_fixed32(pb_encoder_t *enc, uint8_t field_num, uint32_t value)
{
    if (!enc || !enc->buf || field_num == 0) return PB_ERR_PARAM;
    pb_err_t err = pb_put_tag(enc, field_num, PB_WIRE_32BIT);
    if (err != PB_OK) return err;
    if ((uint16_t)(enc->capacity - enc->pos) < 4) return PB_ERR_OVERFLOW;
    enc->buf[enc->pos++] = (uint8_t)(value);
    enc->buf[enc->pos++] = (uint8_t)(value >> 8);
    enc->buf[enc->pos++] = (uint8_t)(value >> 16);
    enc->buf[enc->pos++] = (uint8_t)(value >> 24);
    return PB_OK;
}

pb_err_t pb_encode_fixed64(pb_encoder_t *enc, uint8_t field_num, uint64_t value)
{
    if (!enc || !enc->buf || field_num == 0) return PB_ERR_PARAM;
    pb_err_t err = pb_put_tag(enc, field_num, PB_WIRE_64BIT);
    if (err != PB_OK) return err;
    if ((uint16_t)(enc->capacity - enc->pos) < 8) return PB_ERR_OVERFLOW;
    for (int i = 0; i < 8; i++) {
        enc->buf[enc->pos++] = (uint8_t)(value >> (unsigned)(i * 8));
    }
    return PB_OK;
}

pb_err_t pb_encode_float(pb_encoder_t *enc, uint8_t field_num, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return pb_encode_fixed32(enc, field_num, bits);
}

pb_err_t pb_encode_bytes(pb_encoder_t *enc, uint8_t field_num,
                         const uint8_t *data, uint16_t len)
{
    if (!enc || !enc->buf || field_num == 0) return PB_ERR_PARAM;
    if (!data && len > 0) return PB_ERR_PARAM;
    pb_err_t err = pb_put_tag(enc, field_num, PB_WIRE_LENGTH);
    if (err != PB_OK) return err;
    err = pb_put_varint(enc, len);
    if (err != PB_OK) return err;
    if ((uint16_t)(enc->capacity - enc->pos) < len) return PB_ERR_OVERFLOW;
    if (len > 0) {
        memcpy(&enc->buf[enc->pos], data, len);
        enc->pos += len;
    }
    return PB_OK;
}

pb_err_t pb_encode_string(pb_encoder_t *enc, uint8_t field_num, const char *str)
{
    if (!str) return PB_ERR_PARAM;
    size_t slen = strlen(str);
    if (slen > UINT16_MAX) return PB_ERR_OVERFLOW;
    return pb_encode_bytes(enc, field_num, (const uint8_t *)str, (uint16_t)slen);
}

uint16_t pb_encoder_len(const pb_encoder_t *enc)
{
    if (!enc) return 0;
    return enc->pos;
}

/* ---- Decoder ---- */

void pb_decoder_init(pb_decoder_t *dec, const uint8_t *buf, uint16_t len)
{
    if (!dec) return;
    dec->buf = buf;
    dec->len = len;
    dec->pos = 0;
    ENSURE(dec->pos == 0);
}

pb_err_t pb_decode_field(pb_decoder_t *dec, pb_field_t *field)
{
    if (!dec || !dec->buf || !field) return PB_ERR_PARAM;
    if (dec->pos >= dec->len) return PB_ERR_TRUNCATED;

    /* Decode tag */
    uint64_t tag;
    pb_err_t err = pb_get_varint(dec, &tag);
    if (err != PB_OK) return err;

    /* Protobuf forbids field 0, and field_num cannot hold a field above 255. */
    uint64_t field_num = tag >> 3;
    if (field_num == 0 || field_num > UINT8_MAX) return PB_ERR_MALFORMED;

    field->field_num = (uint8_t)field_num;
    field->wire_type = (pb_wire_type_t)(tag & 0x07);

    switch (field->wire_type) {
    case PB_WIRE_VARINT:
        return pb_get_varint(dec, &field->value.varint);

    case PB_WIRE_64BIT:
        if ((uint16_t)(dec->len - dec->pos) < 8) return PB_ERR_TRUNCATED;
        field->value.fixed64 = 0;
        for (int i = 0; i < 8; i++) {
            field->value.fixed64 |= (uint64_t)dec->buf[dec->pos++]
                                    << (unsigned)(i * 8);
        }
        return PB_OK;

    case PB_WIRE_32BIT:
        if ((uint16_t)(dec->len - dec->pos) < 4) return PB_ERR_TRUNCATED;
        field->value.fixed32 = 0;
        for (int i = 0; i < 4; i++) {
            field->value.fixed32 |= (uint32_t)dec->buf[dec->pos++]
                                    << (unsigned)(i * 8);
        }
        return PB_OK;

    case PB_WIRE_LENGTH: {
        uint64_t length;
        err = pb_get_varint(dec, &length);
        if (err != PB_OK) return err;
        if (length > UINT16_MAX) return PB_ERR_MALFORMED;
        if (length > (uint64_t)(dec->len - dec->pos)) return PB_ERR_TRUNCATED;
        field->value.bytes.data = &dec->buf[dec->pos];
        field->value.bytes.len = (uint16_t)length;
        dec->pos += (uint16_t)length;
        return PB_OK;
    }

    default:
        return PB_ERR_MALFORMED;
    }
}

bool pb_decoder_done(const pb_decoder_t *dec)
{
    if (!dec) return true;
    return dec->pos >= dec->len;
}

int32_t pb_zigzag_decode32(uint32_t n)
{
    return (int32_t)((n >> 1) ^ (uint32_t)(-(int32_t)(n & 1)));
}

int64_t pb_zigzag_decode64(uint64_t n)
{
    return (int64_t)((n >> 1) ^ (uint64_t)(-(int64_t)(n & 1)));
}
