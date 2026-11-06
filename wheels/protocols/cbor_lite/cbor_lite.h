// SPDX-License-Identifier: MIT
/*
 * cbor_lite — lightweight CBOR encoder/decoder (RFC 7049)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Minimal CBOR codec for embedded use. Covers major types 0-7:
 * unsigned int, negative int, byte string, text string, array,
 * map, tag (skip), simple (bool/null/float). No heap, no OS deps.
 * Thread safety: all functions are pure / reentrant (no global state).
 */

#ifndef CBOR_LITE_H
#define CBOR_LITE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Error codes                                                       */
/* ------------------------------------------------------------------ */

typedef enum {
    CBOR_LITE_OK           =  0,
    CBOR_LITE_ERR_PARAM    = -1,
    CBOR_LITE_ERR_OVERFLOW = -2,   /* buffer too small */
    CBOR_LITE_ERR_SHORT    = -3,   /* input truncated  */
    CBOR_LITE_ERR_MALFORMED = -4,  /* invalid encoding */
} cbor_lite_err_t;

/* ------------------------------------------------------------------ */
/*  CBOR major types (high 3 bits)                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    CBOR_MAJOR_UINT   = 0,  /* major type 0: unsigned integer */
    CBOR_MAJOR_NINT   = 1,  /* major type 1: negative integer */
    CBOR_MAJOR_BSTR   = 2,  /* major type 2: byte string      */
    CBOR_MAJOR_TSTR   = 3,  /* major type 3: text string      */
    CBOR_MAJOR_ARRAY  = 4,  /* major type 4: array            */
    CBOR_MAJOR_MAP    = 5,  /* major type 5: map              */
    CBOR_MAJOR_TAG    = 6,  /* major type 6: tag              */
    CBOR_MAJOR_SIMPLE = 7,  /* major type 7: simple / float   */
} cbor_lite_major_t;

/* Simple values (major 7, additional info < 24) */
#define CBOR_SIMPLE_FALSE  20
#define CBOR_SIMPLE_TRUE   21
#define CBOR_SIMPLE_NULL   22

/* ------------------------------------------------------------------ */
/*  Encoder                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t *buf;
    size_t   cap;
    size_t   pos;
    bool     overflow;
} cbor_lite_encoder_t;

/** Initialize encoder over caller-provided buffer. */
void cbor_lite_enc_init(cbor_lite_encoder_t *enc, uint8_t *buf, size_t cap);

/** Return bytes written so far (0 if overflow occurred). */
size_t cbor_lite_enc_len(const cbor_lite_encoder_t *enc);

/** Encode unsigned integer. @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_uint(cbor_lite_encoder_t *enc, uint64_t val);
/** Encode negative integer. @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_nint(cbor_lite_encoder_t *enc, uint64_t val);
/** Encode byte string. @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_bstr(cbor_lite_encoder_t *enc, const uint8_t *data, size_t len);
/** Encode text string. @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_tstr(cbor_lite_encoder_t *enc, const char *str, size_t len);
/** Encode array header (count items follow). @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_array(cbor_lite_encoder_t *enc, size_t count);
/** Encode map header (count key-value pairs follow). @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_map(cbor_lite_encoder_t *enc, size_t count);
/** Encode boolean. @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_bool(cbor_lite_encoder_t *enc, bool val);
/** Encode null. @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_null(cbor_lite_encoder_t *enc);
/** Encode float32. @thread_safe: yes */
cbor_lite_err_t cbor_lite_enc_float(cbor_lite_encoder_t *enc, float val);

/* ------------------------------------------------------------------ */
/*  Decoder                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    cbor_lite_major_t major;
    uint64_t          uval;       /* uint / nint raw / bstr-len / tstr-len / array-count / map-count */
    const uint8_t    *str;        /* pointer into input for bstr/tstr data (NULL otherwise) */
    size_t            str_len;    /* bstr/tstr byte length */
    bool              bool_val;   /* for simple true/false */
    float             float_val;  /* for float32 */
    uint8_t           simple;     /* raw simple value (20=false,21=true,22=null) */
} cbor_lite_item_t;

typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         pos;
} cbor_lite_decoder_t;

/** Initialize decoder over read-only input buffer. */
void cbor_lite_dec_init(cbor_lite_decoder_t *dec, const uint8_t *buf, size_t len);

/** Decode the next CBOR item. A float16 or float64 is skipped and returns
 *  CBOR_LITE_ERR_MALFORMED, as only float32 is supported.
 *  @thread_safe: yes (operates only on *dec) */
cbor_lite_err_t cbor_lite_dec_next(cbor_lite_decoder_t *dec, cbor_lite_item_t *item);

/** Return number of bytes consumed so far. */
size_t cbor_lite_dec_offset(const cbor_lite_decoder_t *dec);

#ifdef __cplusplus
}
#endif

#endif /* CBOR_LITE_H */
