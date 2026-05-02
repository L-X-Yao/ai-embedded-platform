// SPDX-License-Identifier: MIT
/*
 * msgpack — MessagePack binary serialization encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Implements the MessagePack spec (fixint/fixstr/fixarray/fixmap + 8/16/32).
 * 64-bit integers and float64 intentionally omitted — MCU payloads rarely
 * need them and this keeps the wheel under 200 lines.
 */

#include "msgpack.h"
#include <string.h>

/* ---- format byte constants (from MessagePack spec) ---- */
#define FMT_NIL         0xc0
#define FMT_FALSE       0xc2
#define FMT_TRUE        0xc3
#define FMT_BIN8        0xc4
#define FMT_BIN16       0xc5
#define FMT_BIN32       0xc6
#define FMT_FLOAT32     0xca
#define FMT_UINT8       0xcc
#define FMT_UINT16      0xcd
#define FMT_UINT32      0xce
#define FMT_INT8        0xd0
#define FMT_INT16       0xd1
#define FMT_INT32       0xd2
#define FMT_STR8        0xd9
#define FMT_STR16       0xda
#define FMT_STR32       0xdb
#define FMT_ARRAY16     0xdc
#define FMT_ARRAY32     0xdd
#define FMT_MAP16       0xde
#define FMT_MAP32       0xdf

/* ---- helpers ---- */

static inline bool enc_has(const msgpack_enc_t *e, size_t n)
{
    return (e->cap - e->pos) >= n;
}

static inline void enc_u8(msgpack_enc_t *e, uint8_t v)
{
    e->buf[e->pos++] = v;
}

static inline void enc_u16(msgpack_enc_t *e, uint16_t v)
{
    e->buf[e->pos++] = (uint8_t)(v >> 8);
    e->buf[e->pos++] = (uint8_t)(v);
}

static inline void enc_u32(msgpack_enc_t *e, uint32_t v)
{
    e->buf[e->pos++] = (uint8_t)(v >> 24);
    e->buf[e->pos++] = (uint8_t)(v >> 16);
    e->buf[e->pos++] = (uint8_t)(v >> 8);
    e->buf[e->pos++] = (uint8_t)(v);
}

/* ---- encoder ---- */

msgpack_err_t msgpack_enc_init(msgpack_enc_t *enc,
                               uint8_t *buf, size_t cap)
{
    if (!enc || !buf) return MSGPACK_ERR_PARAM;
    enc->buf = buf;
    enc->cap = cap;
    enc->pos = 0;
    return MSGPACK_OK;
}

size_t msgpack_enc_len(const msgpack_enc_t *enc)
{
    return enc ? enc->pos : 0;
}

msgpack_err_t msgpack_enc_nil(msgpack_enc_t *enc)
{
    if (!enc) return MSGPACK_ERR_PARAM;
    if (!enc_has(enc, 1)) return MSGPACK_ERR_OVERFLOW;
    enc_u8(enc, FMT_NIL);
    return MSGPACK_OK;
}

msgpack_err_t msgpack_enc_bool(msgpack_enc_t *enc, bool val)
{
    if (!enc) return MSGPACK_ERR_PARAM;
    if (!enc_has(enc, 1)) return MSGPACK_ERR_OVERFLOW;
    enc_u8(enc, val ? FMT_TRUE : FMT_FALSE);
    return MSGPACK_OK;
}

msgpack_err_t msgpack_enc_uint(msgpack_enc_t *enc, uint32_t val)
{
    if (!enc) return MSGPACK_ERR_PARAM;
    if (val <= 0x7f) {
        if (!enc_has(enc, 1)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, (uint8_t)val);             /* positive fixint */
    } else if (val <= 0xff) {
        if (!enc_has(enc, 2)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_UINT8);
        enc_u8(enc, (uint8_t)val);
    } else if (val <= 0xffff) {
        if (!enc_has(enc, 3)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_UINT16);
        enc_u16(enc, (uint16_t)val);
    } else {
        if (!enc_has(enc, 5)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_UINT32);
        enc_u32(enc, val);
    }
    return MSGPACK_OK;
}

msgpack_err_t msgpack_enc_int(msgpack_enc_t *enc, int32_t val)
{
    if (!enc) return MSGPACK_ERR_PARAM;
    if (val >= 0) return msgpack_enc_uint(enc, (uint32_t)val);
    if (val >= -32) {
        if (!enc_has(enc, 1)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, (uint8_t)(val & 0xff));    /* negative fixint */
    } else if (val >= -128) {
        if (!enc_has(enc, 2)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_INT8);
        enc_u8(enc, (uint8_t)(val & 0xff));
    } else if (val >= -32768) {
        if (!enc_has(enc, 3)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_INT16);
        enc_u16(enc, (uint16_t)(val & 0xffff));
    } else {
        if (!enc_has(enc, 5)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_INT32);
        enc_u32(enc, (uint32_t)val);
    }
    return MSGPACK_OK;
}

msgpack_err_t msgpack_enc_float(msgpack_enc_t *enc, float val)
{
    if (!enc) return MSGPACK_ERR_PARAM;
    if (!enc_has(enc, 5)) return MSGPACK_ERR_OVERFLOW;
    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));
    enc_u8(enc, FMT_FLOAT32);
    enc_u32(enc, bits);
    return MSGPACK_OK;
}

msgpack_err_t msgpack_enc_str(msgpack_enc_t *enc,
                              const char *str, size_t len)
{
    if (!enc || (!str && len > 0)) return MSGPACK_ERR_PARAM;
    if (len <= 31) {
        if (!enc_has(enc, 1 + len)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, (uint8_t)(0xa0u | len));   /* fixstr */
    } else if (len <= 0xff) {
        if (!enc_has(enc, 2 + len)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_STR8);
        enc_u8(enc, (uint8_t)len);
    } else if (len <= 0xffff) {
        if (!enc_has(enc, 3 + len)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_STR16);
        enc_u16(enc, (uint16_t)len);
    } else {
        if (!enc_has(enc, 5 + len)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_STR32);
        enc_u32(enc, (uint32_t)len);
    }
    memcpy(&enc->buf[enc->pos], str, len);
    enc->pos += len;
    return MSGPACK_OK;
}

msgpack_err_t msgpack_enc_bin(msgpack_enc_t *enc,
                              const uint8_t *data, size_t len)
{
    if (!enc || (!data && len > 0)) return MSGPACK_ERR_PARAM;
    if (len <= 0xff) {
        if (!enc_has(enc, 2 + len)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_BIN8);
        enc_u8(enc, (uint8_t)len);
    } else if (len <= 0xffff) {
        if (!enc_has(enc, 3 + len)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_BIN16);
        enc_u16(enc, (uint16_t)len);
    } else {
        if (!enc_has(enc, 5 + len)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_BIN32);
        enc_u32(enc, (uint32_t)len);
    }
    memcpy(&enc->buf[enc->pos], data, len);
    enc->pos += len;
    return MSGPACK_OK;
}

msgpack_err_t msgpack_enc_array(msgpack_enc_t *enc, uint32_t count)
{
    if (!enc) return MSGPACK_ERR_PARAM;
    if (count <= 15) {
        if (!enc_has(enc, 1)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, (uint8_t)(0x90u | count)); /* fixarray */
    } else if (count <= 0xffff) {
        if (!enc_has(enc, 3)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_ARRAY16);
        enc_u16(enc, (uint16_t)count);
    } else {
        if (!enc_has(enc, 5)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_ARRAY32);
        enc_u32(enc, count);
    }
    return MSGPACK_OK;
}

msgpack_err_t msgpack_enc_map(msgpack_enc_t *enc, uint32_t count)
{
    if (!enc) return MSGPACK_ERR_PARAM;
    if (count <= 15) {
        if (!enc_has(enc, 1)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, (uint8_t)(0x80u | count)); /* fixmap */
    } else if (count <= 0xffff) {
        if (!enc_has(enc, 3)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_MAP16);
        enc_u16(enc, (uint16_t)count);
    } else {
        if (!enc_has(enc, 5)) return MSGPACK_ERR_OVERFLOW;
        enc_u8(enc, FMT_MAP32);
        enc_u32(enc, count);
    }
    return MSGPACK_OK;
}

/* ---- decoder helpers ---- */

static inline bool dec_has(const msgpack_dec_t *d, size_t n)
{
    return (d->len - d->pos) >= n;
}

static inline uint8_t dec_u8(msgpack_dec_t *d)
{
    return d->buf[d->pos++];
}

static inline uint16_t dec_u16(msgpack_dec_t *d)
{
    uint16_t v = (uint16_t)((uint16_t)d->buf[d->pos] << 8 |
                             d->buf[d->pos + 1]);
    d->pos += 2;
    return v;
}

static inline uint32_t dec_u32(msgpack_dec_t *d)
{
    uint32_t v = (uint32_t)d->buf[d->pos]     << 24 |
                 (uint32_t)d->buf[d->pos + 1]  << 16 |
                 (uint32_t)d->buf[d->pos + 2]  << 8  |
                 (uint32_t)d->buf[d->pos + 3];
    d->pos += 4;
    return v;
}

/* ---- decoder ---- */

msgpack_err_t msgpack_dec_init(msgpack_dec_t *dec,
                               const uint8_t *buf, size_t len)
{
    if (!dec || (!buf && len > 0)) return MSGPACK_ERR_PARAM;
    dec->buf = buf;
    dec->len = len;
    dec->pos = 0;
    return MSGPACK_OK;
}

msgpack_err_t msgpack_dec_next(msgpack_dec_t *dec,
                               msgpack_token_t *tok)
{
    if (!dec || !tok) return MSGPACK_ERR_PARAM;
    if (!dec_has(dec, 1)) return MSGPACK_ERR_SHORT;

    uint8_t hdr = dec_u8(dec);

    /* positive fixint 0x00..0x7f */
    if (hdr <= 0x7f) {
        tok->type = MSGPACK_TYPE_UINT;
        tok->val.u32 = hdr;
        return MSGPACK_OK;
    }
    /* fixmap 0x80..0x8f */
    if ((hdr & 0xf0) == 0x80) {
        tok->type = MSGPACK_TYPE_MAP;
        tok->val.map_count = hdr & 0x0fu;
        return MSGPACK_OK;
    }
    /* fixarray 0x90..0x9f */
    if ((hdr & 0xf0) == 0x90) {
        tok->type = MSGPACK_TYPE_ARRAY;
        tok->val.array_count = hdr & 0x0fu;
        return MSGPACK_OK;
    }
    /* fixstr 0xa0..0xbf */
    if ((hdr & 0xe0) == 0xa0) {
        size_t slen = hdr & 0x1fu;
        if (!dec_has(dec, slen)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_STR;
        tok->val.str.ptr = &dec->buf[dec->pos];
        tok->val.str.len = slen;
        dec->pos += slen;
        return MSGPACK_OK;
    }
    /* negative fixint 0xe0..0xff */
    if (hdr >= 0xe0) {
        tok->type = MSGPACK_TYPE_INT;
        tok->val.i32 = (int32_t)(int8_t)hdr;
        return MSGPACK_OK;
    }

    switch (hdr) {
    case FMT_NIL:
        tok->type = MSGPACK_TYPE_NIL;
        return MSGPACK_OK;
    case FMT_FALSE:
        tok->type = MSGPACK_TYPE_BOOL;
        tok->val.boolean = false;
        return MSGPACK_OK;
    case FMT_TRUE:
        tok->type = MSGPACK_TYPE_BOOL;
        tok->val.boolean = true;
        return MSGPACK_OK;

    /* unsigned integers */
    case FMT_UINT8:
        if (!dec_has(dec, 1)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_UINT;
        tok->val.u32 = dec_u8(dec);
        return MSGPACK_OK;
    case FMT_UINT16:
        if (!dec_has(dec, 2)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_UINT;
        tok->val.u32 = dec_u16(dec);
        return MSGPACK_OK;
    case FMT_UINT32:
        if (!dec_has(dec, 4)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_UINT;
        tok->val.u32 = dec_u32(dec);
        return MSGPACK_OK;

    /* signed integers */
    case FMT_INT8:
        if (!dec_has(dec, 1)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_INT;
        tok->val.i32 = (int32_t)(int8_t)dec_u8(dec);
        return MSGPACK_OK;
    case FMT_INT16:
        if (!dec_has(dec, 2)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_INT;
        tok->val.i32 = (int32_t)(int16_t)dec_u16(dec);
        return MSGPACK_OK;
    case FMT_INT32:
        if (!dec_has(dec, 4)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_INT;
        tok->val.i32 = (int32_t)dec_u32(dec);
        return MSGPACK_OK;

    /* float32 */
    case FMT_FLOAT32:
        if (!dec_has(dec, 4)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_FLOAT;
        { uint32_t bits = dec_u32(dec); memcpy(&tok->val.f32, &bits, 4); }
        return MSGPACK_OK;

    /* string */
    case FMT_STR8: {
        if (!dec_has(dec, 1)) return MSGPACK_ERR_SHORT;
        size_t slen = dec_u8(dec);
        if (!dec_has(dec, slen)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_STR;
        tok->val.str.ptr = &dec->buf[dec->pos];
        tok->val.str.len = slen;
        dec->pos += slen;
        return MSGPACK_OK;
    }
    case FMT_STR16: {
        if (!dec_has(dec, 2)) return MSGPACK_ERR_SHORT;
        size_t slen = dec_u16(dec);
        if (!dec_has(dec, slen)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_STR;
        tok->val.str.ptr = &dec->buf[dec->pos];
        tok->val.str.len = slen;
        dec->pos += slen;
        return MSGPACK_OK;
    }
    case FMT_STR32: {
        if (!dec_has(dec, 4)) return MSGPACK_ERR_SHORT;
        size_t slen = dec_u32(dec);
        if (!dec_has(dec, slen)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_STR;
        tok->val.str.ptr = &dec->buf[dec->pos];
        tok->val.str.len = slen;
        dec->pos += slen;
        return MSGPACK_OK;
    }

    /* binary */
    case FMT_BIN8: {
        if (!dec_has(dec, 1)) return MSGPACK_ERR_SHORT;
        size_t blen = dec_u8(dec);
        if (!dec_has(dec, blen)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_BIN;
        tok->val.bin.ptr = &dec->buf[dec->pos];
        tok->val.bin.len = blen;
        dec->pos += blen;
        return MSGPACK_OK;
    }
    case FMT_BIN16: {
        if (!dec_has(dec, 2)) return MSGPACK_ERR_SHORT;
        size_t blen = dec_u16(dec);
        if (!dec_has(dec, blen)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_BIN;
        tok->val.bin.ptr = &dec->buf[dec->pos];
        tok->val.bin.len = blen;
        dec->pos += blen;
        return MSGPACK_OK;
    }
    case FMT_BIN32: {
        if (!dec_has(dec, 4)) return MSGPACK_ERR_SHORT;
        size_t blen = dec_u32(dec);
        if (!dec_has(dec, blen)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_BIN;
        tok->val.bin.ptr = &dec->buf[dec->pos];
        tok->val.bin.len = blen;
        dec->pos += blen;
        return MSGPACK_OK;
    }

    /* array */
    case FMT_ARRAY16:
        if (!dec_has(dec, 2)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_ARRAY;
        tok->val.array_count = dec_u16(dec);
        return MSGPACK_OK;
    case FMT_ARRAY32:
        if (!dec_has(dec, 4)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_ARRAY;
        tok->val.array_count = dec_u32(dec);
        return MSGPACK_OK;

    /* map */
    case FMT_MAP16:
        if (!dec_has(dec, 2)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_MAP;
        tok->val.map_count = dec_u16(dec);
        return MSGPACK_OK;
    case FMT_MAP32:
        if (!dec_has(dec, 4)) return MSGPACK_ERR_SHORT;
        tok->type = MSGPACK_TYPE_MAP;
        tok->val.map_count = dec_u32(dec);
        return MSGPACK_OK;

    default:
        return MSGPACK_ERR_FORMAT;
    }
}
