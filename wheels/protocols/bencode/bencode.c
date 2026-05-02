/*
 * bencode — Bencode encoder/decoder (BitTorrent encoding format)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 */

#include "bencode.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static bencode_err_t enc_byte(bencode_encoder_t *enc, uint8_t b)
{
    if (enc->pos >= enc->cap)
        return BENCODE_ERR_OVERFLOW;
    enc->buf[enc->pos++] = b;
    return BENCODE_OK;
}

static bencode_err_t enc_bytes(bencode_encoder_t *enc,
                               const uint8_t *data, size_t len)
{
    if (enc->pos + len > enc->cap)
        return BENCODE_ERR_OVERFLOW;
    memcpy(&enc->buf[enc->pos], data, len);
    enc->pos += len;
    return BENCODE_OK;
}

/* Write decimal representation of a non-negative value. */
static bencode_err_t enc_uint64(bencode_encoder_t *enc, uint64_t v)
{
    char tmp[21]; /* max uint64 = 20 digits + NUL */
    int i = 0;
    if (v == 0) {
        tmp[i++] = '0';
    } else {
        while (v > 0) {
            tmp[i++] = (char)('0' + (int)(v % 10));
            v /= 10;
        }
    }
    /* reverse */
    for (int j = i - 1; j >= 0; j--) {
        bencode_err_t err = enc_byte(enc, (uint8_t)tmp[j]);
        if (err != BENCODE_OK)
            return err;
    }
    return BENCODE_OK;
}

/* ------------------------------------------------------------------ */
/*  Encoder                                                            */
/* ------------------------------------------------------------------ */

bencode_err_t bencode_encoder_init(bencode_encoder_t *enc,
                                   uint8_t *buf, size_t cap)
{
    if (!enc || !buf)
        return BENCODE_ERR_PARAM;
    enc->buf = buf;
    enc->cap = cap;
    enc->pos = 0;
    return BENCODE_OK;
}

bencode_err_t bencode_encode_int(bencode_encoder_t *enc, int64_t value)
{
    if (!enc)
        return BENCODE_ERR_PARAM;

    bencode_err_t err = enc_byte(enc, (uint8_t)'i');
    if (err != BENCODE_OK) return err;

    if (value < 0) {
        err = enc_byte(enc, (uint8_t)'-');
        if (err != BENCODE_OK) return err;
        /* handle INT64_MIN: -(INT64_MIN) overflows int64,
         * but cast to uint64 gives correct magnitude */
        uint64_t mag = (value == INT64_MIN)
                     ? ((uint64_t)INT64_MAX + 1u)
                     : (uint64_t)(-value);
        err = enc_uint64(enc, mag);
    } else {
        err = enc_uint64(enc, (uint64_t)value);
    }
    if (err != BENCODE_OK) return err;

    return enc_byte(enc, (uint8_t)'e');
}

bencode_err_t bencode_encode_string(bencode_encoder_t *enc,
                                    const uint8_t *data, size_t len)
{
    if (!enc)
        return BENCODE_ERR_PARAM;
    if (len > 0 && !data)
        return BENCODE_ERR_PARAM;

    bencode_err_t err = enc_uint64(enc, (uint64_t)len);
    if (err != BENCODE_OK) return err;

    err = enc_byte(enc, (uint8_t)':');
    if (err != BENCODE_OK) return err;

    if (len > 0) {
        err = enc_bytes(enc, data, len);
        if (err != BENCODE_OK) return err;
    }

    return BENCODE_OK;
}

bencode_err_t bencode_encode_list_begin(bencode_encoder_t *enc)
{
    if (!enc) return BENCODE_ERR_PARAM;
    return enc_byte(enc, (uint8_t)'l');
}

bencode_err_t bencode_encode_list_end(bencode_encoder_t *enc)
{
    if (!enc) return BENCODE_ERR_PARAM;
    return enc_byte(enc, (uint8_t)'e');
}

bencode_err_t bencode_encode_dict_begin(bencode_encoder_t *enc)
{
    if (!enc) return BENCODE_ERR_PARAM;
    return enc_byte(enc, (uint8_t)'d');
}

bencode_err_t bencode_encode_dict_end(bencode_encoder_t *enc)
{
    if (!enc) return BENCODE_ERR_PARAM;
    return enc_byte(enc, (uint8_t)'e');
}

size_t bencode_encoder_len(const bencode_encoder_t *enc)
{
    if (!enc) return 0;
    return enc->pos;
}

/* ------------------------------------------------------------------ */
/*  Decoder                                                            */
/* ------------------------------------------------------------------ */

bencode_err_t bencode_decoder_init(bencode_decoder_t *dec,
                                   const uint8_t *data, size_t len)
{
    if (!dec || !data)
        return BENCODE_ERR_PARAM;
    dec->data  = data;
    dec->len   = len;
    dec->pos   = 0;
    dec->depth = 0;
    return BENCODE_OK;
}

bencode_err_t bencode_decode_next(bencode_decoder_t *dec,
                                  bencode_token_t *token)
{
    if (!dec || !token)
        return BENCODE_ERR_PARAM;

    if (dec->pos >= dec->len)
        return BENCODE_ERR_END;

    uint8_t ch = dec->data[dec->pos];

    /* Integer: i<digits>e */
    if (ch == 'i') {
        dec->pos++;
        if (dec->pos >= dec->len)
            return BENCODE_ERR_MALFORMED;

        bool negative = false;
        if (dec->data[dec->pos] == '-') {
            negative = true;
            dec->pos++;
            if (dec->pos >= dec->len)
                return BENCODE_ERR_MALFORMED;
            /* i-0e is invalid per spec */
            if (dec->data[dec->pos] == '0')
                return BENCODE_ERR_MALFORMED;
        }

        if (dec->data[dec->pos] < '0' || dec->data[dec->pos] > '9')
            return BENCODE_ERR_MALFORMED;

        /* leading zeros forbidden (except bare "i0e") */
        bool leading_zero = (dec->data[dec->pos] == '0');

        uint64_t mag = 0;
        size_t digits = 0;
        while (dec->pos < dec->len &&
               dec->data[dec->pos] >= '0' &&
               dec->data[dec->pos] <= '9') {
            uint8_t d = dec->data[dec->pos] - '0';
            /* overflow check */
            if (mag > (UINT64_MAX - d) / 10)
                return BENCODE_ERR_MALFORMED;
            mag = mag * 10 + d;
            dec->pos++;
            digits++;
        }

        if (leading_zero && digits > 1)
            return BENCODE_ERR_MALFORMED;

        if (dec->pos >= dec->len || dec->data[dec->pos] != 'e')
            return BENCODE_ERR_MALFORMED;
        dec->pos++;

        token->type = BENCODE_TOKEN_INT;
        if (negative) {
            /* INT64_MIN magnitude = 9223372036854775808 */
            if (mag > (uint64_t)INT64_MAX + 1u)
                return BENCODE_ERR_MALFORMED;
            if (mag == (uint64_t)INT64_MAX + 1u)
                token->value.integer = INT64_MIN;
            else
                token->value.integer = -(int64_t)mag;
        } else {
            if (mag > (uint64_t)INT64_MAX)
                return BENCODE_ERR_MALFORMED;
            token->value.integer = (int64_t)mag;
        }
        return BENCODE_OK;
    }

    /* List begin */
    if (ch == 'l') {
        dec->pos++;
        if (dec->depth < BENCODE_MAX_DEPTH)
            dec->nesting[dec->depth] = 'l';
        dec->depth++;
        token->type = BENCODE_TOKEN_LIST_BEGIN;
        return BENCODE_OK;
    }

    /* Dict begin */
    if (ch == 'd') {
        dec->pos++;
        if (dec->depth < BENCODE_MAX_DEPTH)
            dec->nesting[dec->depth] = 'd';
        dec->depth++;
        token->type = BENCODE_TOKEN_DICT_BEGIN;
        return BENCODE_OK;
    }

    /* End marker (list or dict) — use nesting stack to distinguish */
    if (ch == 'e') {
        dec->pos++;
        if (dec->depth > 0) {
            dec->depth--;
            if (dec->depth < BENCODE_MAX_DEPTH &&
                dec->nesting[dec->depth] == 'd') {
                token->type = BENCODE_TOKEN_DICT_END;
            } else {
                token->type = BENCODE_TOKEN_LIST_END;
            }
        } else {
            token->type = BENCODE_TOKEN_LIST_END;
        }
        return BENCODE_OK;
    }

    /* String: <len>:<data> */
    if (ch >= '0' && ch <= '9') {
        /* leading zeros in length are technically allowed by some
         * implementations, but we parse them normally */
        uint64_t slen = 0;
        while (dec->pos < dec->len &&
               dec->data[dec->pos] >= '0' &&
               dec->data[dec->pos] <= '9') {
            uint8_t d = dec->data[dec->pos] - '0';
            if (slen > (UINT64_MAX - d) / 10)
                return BENCODE_ERR_MALFORMED;
            slen = slen * 10 + d;
            dec->pos++;
        }

        if (dec->pos >= dec->len || dec->data[dec->pos] != ':')
            return BENCODE_ERR_MALFORMED;
        dec->pos++;

        if (slen > dec->len - dec->pos)
            return BENCODE_ERR_MALFORMED;

        token->type = BENCODE_TOKEN_STRING;
        token->value.string.data = &dec->data[dec->pos];
        token->value.string.len  = (size_t)slen;
        dec->pos += (size_t)slen;
        return BENCODE_OK;
    }

    return BENCODE_ERR_MALFORMED;
}
