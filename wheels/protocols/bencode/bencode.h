// SPDX-License-Identifier: MIT
/*
 * bencode — Bencode encoder/decoder (BitTorrent encoding format)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Stateless encoder/decoder for Bencode format: integers "i42e",
 * strings "3:foo", lists "l...e", dicts "d...e".
 * Encoder writes tokens to a caller-provided buffer.
 * Decoder is an iterative SAX-style token parser.
 * Pure C99, no heap, no OS dependency.
 * Thread safety: all functions are pure / reentrant (no global state).
 */

#ifndef BENCODE_H
#define BENCODE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Error codes                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    BENCODE_OK             =  0,
    BENCODE_ERR_PARAM      = -1,
    BENCODE_ERR_OVERFLOW   = -2,
    BENCODE_ERR_MALFORMED  = -3,
    BENCODE_ERR_END        = -4,   /* no more tokens */
} bencode_err_t;

/* ------------------------------------------------------------------ */
/*  Encoder — write tokens into a flat buffer                          */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t *buf;
    size_t   cap;
    size_t   pos;
} bencode_encoder_t;

/** Initialize encoder with caller-provided buffer.
 *  @thread_safe: yes (pure function) */
bencode_err_t bencode_encoder_init(bencode_encoder_t *enc,
                                   uint8_t *buf, size_t cap);

/** Encode an integer: i<value>e.
 *  @thread_safe: caller_must_lock */
bencode_err_t bencode_encode_int(bencode_encoder_t *enc, int64_t value);

/** Encode a byte string: <len>:<data>.
 *  @thread_safe: caller_must_lock */
bencode_err_t bencode_encode_string(bencode_encoder_t *enc,
                                    const uint8_t *data, size_t len);

/** Write list begin marker 'l'.
 *  @thread_safe: caller_must_lock */
bencode_err_t bencode_encode_list_begin(bencode_encoder_t *enc);

/** Write list/dict end marker 'e'.
 *  @thread_safe: caller_must_lock */
bencode_err_t bencode_encode_list_end(bencode_encoder_t *enc);

/** Write dict begin marker 'd'.
 *  @thread_safe: caller_must_lock */
bencode_err_t bencode_encode_dict_begin(bencode_encoder_t *enc);

/** Write dict end marker 'e' (same as list_end).
 *  @thread_safe: caller_must_lock */
bencode_err_t bencode_encode_dict_end(bencode_encoder_t *enc);

/** Return current encoded length.
 *  @thread_safe: yes (pure function) */
size_t bencode_encoder_len(const bencode_encoder_t *enc);

/* ------------------------------------------------------------------ */
/*  Decoder — iterative SAX-style token parser                         */
/* ------------------------------------------------------------------ */

typedef enum {
    BENCODE_TOKEN_INT        = 0,
    BENCODE_TOKEN_STRING     = 1,
    BENCODE_TOKEN_LIST_BEGIN = 2,
    BENCODE_TOKEN_LIST_END   = 3,
    BENCODE_TOKEN_DICT_BEGIN = 4,
    BENCODE_TOKEN_DICT_END   = 5,
} bencode_token_type_t;

typedef struct {
    bencode_token_type_t type;
    union {
        int64_t        integer;      /* BENCODE_TOKEN_INT */
        struct {
            const uint8_t *data;     /* BENCODE_TOKEN_STRING (zero-copy) */
            size_t         len;
        } string;
    } value;
} bencode_token_t;

#ifndef BENCODE_MAX_DEPTH
#define BENCODE_MAX_DEPTH 16
#endif

typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         pos;
    uint8_t        nesting[BENCODE_MAX_DEPTH]; /* 'l' or 'd' per level */
    uint8_t        depth;
} bencode_decoder_t;

/** Initialize decoder over input data.
 *  @thread_safe: yes (pure function) */
bencode_err_t bencode_decoder_init(bencode_decoder_t *dec,
                                   const uint8_t *data, size_t len);

/** Decode next token. Returns BENCODE_ERR_END when input is exhausted.
 *  @thread_safe: caller_must_lock */
bencode_err_t bencode_decode_next(bencode_decoder_t *dec,
                                  bencode_token_t *token);

#ifdef __cplusplus
}
#endif

#endif /* BENCODE_H */
