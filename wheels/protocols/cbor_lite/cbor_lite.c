/*
 * cbor_lite — lightweight CBOR encoder/decoder (RFC 7049)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#include "cbor_lite.h"
#include <string.h>

/* ================================================================== */
/*  Internal: encode major+argument header                            */
/* ================================================================== */

static cbor_lite_err_t enc_head(cbor_lite_encoder_t *enc,
                                uint8_t major, uint64_t val)
{
    uint8_t mt = (uint8_t)(major << 5);

    if (val <= 23) {
        if (enc->pos + 1 > enc->cap) { enc->overflow = true; return CBOR_LITE_ERR_OVERFLOW; }
        enc->buf[enc->pos++] = (uint8_t)(mt | val);
    } else if (val <= 0xFF) {
        if (enc->pos + 2 > enc->cap) { enc->overflow = true; return CBOR_LITE_ERR_OVERFLOW; }
        enc->buf[enc->pos++] = (uint8_t)(mt | 24);
        enc->buf[enc->pos++] = (uint8_t)val;
    } else if (val <= 0xFFFF) {
        if (enc->pos + 3 > enc->cap) { enc->overflow = true; return CBOR_LITE_ERR_OVERFLOW; }
        enc->buf[enc->pos++] = (uint8_t)(mt | 25);
        enc->buf[enc->pos++] = (uint8_t)(val >> 8);
        enc->buf[enc->pos++] = (uint8_t)val;
    } else if (val <= 0xFFFFFFFFULL) {
        if (enc->pos + 5 > enc->cap) { enc->overflow = true; return CBOR_LITE_ERR_OVERFLOW; }
        enc->buf[enc->pos++] = (uint8_t)(mt | 26);
        enc->buf[enc->pos++] = (uint8_t)(val >> 24);
        enc->buf[enc->pos++] = (uint8_t)(val >> 16);
        enc->buf[enc->pos++] = (uint8_t)(val >> 8);
        enc->buf[enc->pos++] = (uint8_t)val;
    } else {
        if (enc->pos + 9 > enc->cap) { enc->overflow = true; return CBOR_LITE_ERR_OVERFLOW; }
        enc->buf[enc->pos++] = (uint8_t)(mt | 27);
        enc->buf[enc->pos++] = (uint8_t)(val >> 56);
        enc->buf[enc->pos++] = (uint8_t)(val >> 48);
        enc->buf[enc->pos++] = (uint8_t)(val >> 40);
        enc->buf[enc->pos++] = (uint8_t)(val >> 32);
        enc->buf[enc->pos++] = (uint8_t)(val >> 24);
        enc->buf[enc->pos++] = (uint8_t)(val >> 16);
        enc->buf[enc->pos++] = (uint8_t)(val >> 8);
        enc->buf[enc->pos++] = (uint8_t)val;
    }
    return CBOR_LITE_OK;
}

/* ================================================================== */
/*  Encoder public API                                                */
/* ================================================================== */

void cbor_lite_enc_init(cbor_lite_encoder_t *enc, uint8_t *buf, size_t cap)
{
    if (!enc) return;
    enc->buf = buf;
    enc->cap = cap;
    enc->pos = 0;
    enc->overflow = false;
}

size_t cbor_lite_enc_len(const cbor_lite_encoder_t *enc)
{
    if (!enc || enc->overflow) return 0;
    return enc->pos;
}

cbor_lite_err_t cbor_lite_enc_uint(cbor_lite_encoder_t *enc, uint64_t val)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;
    return enc_head(enc, CBOR_MAJOR_UINT, val);
}

cbor_lite_err_t cbor_lite_enc_nint(cbor_lite_encoder_t *enc, uint64_t val)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;
    /* CBOR negative: -1 - val, so encode raw val */
    return enc_head(enc, CBOR_MAJOR_NINT, val);
}

cbor_lite_err_t cbor_lite_enc_bstr(cbor_lite_encoder_t *enc,
                                   const uint8_t *data, size_t len)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;
    if (len > 0 && !data) return CBOR_LITE_ERR_PARAM;

    cbor_lite_err_t rc = enc_head(enc, CBOR_MAJOR_BSTR, (uint64_t)len);
    if (rc != CBOR_LITE_OK) return rc;

    if (enc->pos + len > enc->cap) { enc->overflow = true; return CBOR_LITE_ERR_OVERFLOW; }
    if (len > 0) {
        memcpy(&enc->buf[enc->pos], data, len);
    }
    enc->pos += len;
    return CBOR_LITE_OK;
}

cbor_lite_err_t cbor_lite_enc_tstr(cbor_lite_encoder_t *enc,
                                   const char *str, size_t len)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;
    if (len > 0 && !str) return CBOR_LITE_ERR_PARAM;

    cbor_lite_err_t rc = enc_head(enc, CBOR_MAJOR_TSTR, (uint64_t)len);
    if (rc != CBOR_LITE_OK) return rc;

    if (enc->pos + len > enc->cap) { enc->overflow = true; return CBOR_LITE_ERR_OVERFLOW; }
    if (len > 0) {
        memcpy(&enc->buf[enc->pos], str, len);
    }
    enc->pos += len;
    return CBOR_LITE_OK;
}

cbor_lite_err_t cbor_lite_enc_array(cbor_lite_encoder_t *enc, size_t count)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;
    return enc_head(enc, CBOR_MAJOR_ARRAY, (uint64_t)count);
}

cbor_lite_err_t cbor_lite_enc_map(cbor_lite_encoder_t *enc, size_t count)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;
    return enc_head(enc, CBOR_MAJOR_MAP, (uint64_t)count);
}

cbor_lite_err_t cbor_lite_enc_bool(cbor_lite_encoder_t *enc, bool val)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;
    return enc_head(enc, CBOR_MAJOR_SIMPLE,
                    val ? CBOR_SIMPLE_TRUE : CBOR_SIMPLE_FALSE);
}

cbor_lite_err_t cbor_lite_enc_null(cbor_lite_encoder_t *enc)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;
    return enc_head(enc, CBOR_MAJOR_SIMPLE, CBOR_SIMPLE_NULL);
}

cbor_lite_err_t cbor_lite_enc_float(cbor_lite_encoder_t *enc, float val)
{
    if (!enc) return CBOR_LITE_ERR_PARAM;
    if (enc->overflow) return CBOR_LITE_ERR_OVERFLOW;

    /* IEEE 754 single-precision: major 7, additional 26, 4 bytes */
    if (enc->pos + 5 > enc->cap) { enc->overflow = true; return CBOR_LITE_ERR_OVERFLOW; }

    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));

    enc->buf[enc->pos++] = (uint8_t)((CBOR_MAJOR_SIMPLE << 5) | 26);
    enc->buf[enc->pos++] = (uint8_t)(bits >> 24);
    enc->buf[enc->pos++] = (uint8_t)(bits >> 16);
    enc->buf[enc->pos++] = (uint8_t)(bits >> 8);
    enc->buf[enc->pos++] = (uint8_t)bits;
    return CBOR_LITE_OK;
}

/* ================================================================== */
/*  Internal: decode argument from additional info                    */
/* ================================================================== */

static cbor_lite_err_t dec_argument(const uint8_t *buf, size_t len,
                                    size_t pos, uint8_t ai,
                                    uint64_t *out_val, size_t *out_consumed)
{
    if (ai <= 23) {
        *out_val = ai;
        *out_consumed = 0;
        return CBOR_LITE_OK;
    }
    if (ai == 24) {
        if (pos + 1 > len) return CBOR_LITE_ERR_SHORT;
        *out_val = buf[pos];
        *out_consumed = 1;
        return CBOR_LITE_OK;
    }
    if (ai == 25) {
        if (pos + 2 > len) return CBOR_LITE_ERR_SHORT;
        *out_val = ((uint64_t)buf[pos] << 8) | buf[pos + 1];
        *out_consumed = 2;
        return CBOR_LITE_OK;
    }
    if (ai == 26) {
        if (pos + 4 > len) return CBOR_LITE_ERR_SHORT;
        *out_val = ((uint64_t)buf[pos]     << 24)
                 | ((uint64_t)buf[pos + 1] << 16)
                 | ((uint64_t)buf[pos + 2] << 8)
                 |  (uint64_t)buf[pos + 3];
        *out_consumed = 4;
        return CBOR_LITE_OK;
    }
    if (ai == 27) {
        if (pos + 8 > len) return CBOR_LITE_ERR_SHORT;
        *out_val = ((uint64_t)buf[pos]     << 56)
                 | ((uint64_t)buf[pos + 1] << 48)
                 | ((uint64_t)buf[pos + 2] << 40)
                 | ((uint64_t)buf[pos + 3] << 32)
                 | ((uint64_t)buf[pos + 4] << 24)
                 | ((uint64_t)buf[pos + 5] << 16)
                 | ((uint64_t)buf[pos + 6] << 8)
                 |  (uint64_t)buf[pos + 7];
        *out_consumed = 8;
        return CBOR_LITE_OK;
    }
    /* 28-30 reserved, 31 = indefinite (not supported) */
    return CBOR_LITE_ERR_MALFORMED;
}

/* ================================================================== */
/*  Decoder public API                                                */
/* ================================================================== */

void cbor_lite_dec_init(cbor_lite_decoder_t *dec,
                        const uint8_t *buf, size_t len)
{
    if (!dec) return;
    dec->buf = buf;
    dec->len = len;
    dec->pos = 0;
}

size_t cbor_lite_dec_offset(const cbor_lite_decoder_t *dec)
{
    if (!dec) return 0;
    return dec->pos;
}

cbor_lite_err_t cbor_lite_dec_next(cbor_lite_decoder_t *dec,
                                   cbor_lite_item_t *item)
{
    if (!dec || !item) return CBOR_LITE_ERR_PARAM;
    if (dec->pos >= dec->len) return CBOR_LITE_ERR_SHORT;

    memset(item, 0, sizeof(*item));

    uint8_t initial = dec->buf[dec->pos];
    uint8_t mt = (uint8_t)(initial >> 5);
    uint8_t ai = (uint8_t)(initial & 0x1F);
    dec->pos++;

    item->major = (cbor_lite_major_t)mt;

    /* Major 7 + ai 26 = float32 (special case before generic argument) */
    if (mt == CBOR_MAJOR_SIMPLE && ai == 26) {
        if (dec->pos + 4 > dec->len) return CBOR_LITE_ERR_SHORT;
        uint32_t bits = ((uint32_t)dec->buf[dec->pos]     << 24)
                      | ((uint32_t)dec->buf[dec->pos + 1] << 16)
                      | ((uint32_t)dec->buf[dec->pos + 2] << 8)
                      |  (uint32_t)dec->buf[dec->pos + 3];
        memcpy(&item->float_val, &bits, sizeof(float));
        item->simple = 26;
        dec->pos += 4;
        return CBOR_LITE_OK;
    }

    /* Decode the argument value */
    uint64_t val = 0;
    size_t consumed = 0;
    cbor_lite_err_t rc = dec_argument(dec->buf, dec->len, dec->pos,
                                      ai, &val, &consumed);
    if (rc != CBOR_LITE_OK) return rc;
    dec->pos += consumed;

    item->uval = val;

    switch (mt) {
    case CBOR_MAJOR_UINT:
    case CBOR_MAJOR_NINT:
        break;
    case CBOR_MAJOR_BSTR:
        if (val > (uint64_t)(dec->len - dec->pos)) return CBOR_LITE_ERR_SHORT;
        item->str = &dec->buf[dec->pos];
        item->str_len = (size_t)val;
        dec->pos += (size_t)val;
        break;
    case CBOR_MAJOR_TSTR:
        if (val > (uint64_t)(dec->len - dec->pos)) return CBOR_LITE_ERR_SHORT;
        item->str = &dec->buf[dec->pos];
        item->str_len = (size_t)val;
        dec->pos += (size_t)val;
        break;
    case CBOR_MAJOR_ARRAY:
    case CBOR_MAJOR_MAP:
        /* uval already holds count; items follow inline */
        break;
    case CBOR_MAJOR_TAG:
        /* uval holds tag number; tagged value follows */
        break;
    case CBOR_MAJOR_SIMPLE:
        item->simple = (uint8_t)val;
        if (val == CBOR_SIMPLE_TRUE)  item->bool_val = true;
        if (val == CBOR_SIMPLE_FALSE) item->bool_val = false;
        break;
    default:
        return CBOR_LITE_ERR_MALFORMED;
    }

    return CBOR_LITE_OK;
}
