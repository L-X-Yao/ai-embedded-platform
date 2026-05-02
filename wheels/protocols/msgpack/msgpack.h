/*
 * msgpack — MessagePack binary serialization encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Compact binary JSON alternative for IoT telemetry and configuration.
 * Supports fixint/fixstr/fixarray/fixmap + 8/16/32 variants.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef MSGPACK_H
#define MSGPACK_H

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
    MSGPACK_OK           =  0,
    MSGPACK_ERR_PARAM    = -1,
    MSGPACK_ERR_OVERFLOW = -2,   /* buffer too small for encode       */
    MSGPACK_ERR_SHORT    = -3,   /* input truncated during decode     */
    MSGPACK_ERR_FORMAT   = -4,   /* unsupported/reserved format byte  */
    /*
     * Decoder returns ERR_FORMAT for these MessagePack types:
     *   - float64   (0xcb)  — 64-bit float, rarely needed on MCU
     *   - uint64    (0xcf)  — 64-bit unsigned integer
     *   - int64     (0xd3)  — 64-bit signed integer
     *   - fixext*   (0xd4-0xd8) — fixed-length extension types
     *   - ext8/16/32 (0xc7-0xc9) — variable-length extension types
     *   - timestamp (0xd6/0xd7 with type -1) — extension timestamp
     * These are intentionally omitted to keep code size small for
     * embedded use. Encoder likewise only supports up to 32-bit values.
     */
} msgpack_err_t;

/* ------------------------------------------------------------------ */
/*  Token types (decoder output)                                       */
/* ------------------------------------------------------------------ */

typedef enum {
    MSGPACK_TYPE_NIL,
    MSGPACK_TYPE_BOOL,
    MSGPACK_TYPE_UINT,
    MSGPACK_TYPE_INT,
    MSGPACK_TYPE_FLOAT,
    MSGPACK_TYPE_STR,
    MSGPACK_TYPE_BIN,
    MSGPACK_TYPE_ARRAY,
    MSGPACK_TYPE_MAP,
} msgpack_type_t;

/* ------------------------------------------------------------------ */
/*  Encoder                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t *buf;       /* caller-owned output buffer  */
    size_t   cap;       /* buffer capacity             */
    size_t   pos;       /* current write position      */
} msgpack_enc_t;

/** Initialize encoder with caller-provided buffer.
 *  @thread_safe: yes (distinct enc instances) */
msgpack_err_t msgpack_enc_init(msgpack_enc_t *enc,
                               uint8_t *buf, size_t cap);

/** Return number of bytes written so far.
 *  @thread_safe: yes (read-only) */
size_t        msgpack_enc_len(const msgpack_enc_t *enc);

/** @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_nil(msgpack_enc_t *enc);

/** @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_bool(msgpack_enc_t *enc, bool val);

/** Pack unsigned integer (auto-selects smallest encoding).
 *  @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_uint(msgpack_enc_t *enc, uint32_t val);

/** Pack signed integer (auto-selects smallest encoding).
 *  @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_int(msgpack_enc_t *enc, int32_t val);

/** Pack 32-bit float.
 *  @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_float(msgpack_enc_t *enc, float val);

/** Pack string (length-prefixed, no NUL terminator stored).
 *  @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_str(msgpack_enc_t *enc,
                              const char *str, size_t len);

/** Pack binary blob (length-prefixed).
 *  @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_bin(msgpack_enc_t *enc,
                              const uint8_t *data, size_t len);

/** Write array header (caller packs N elements after this).
 *  @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_array(msgpack_enc_t *enc, uint32_t count);

/** Write map header (caller packs N key-value pairs after this).
 *  @thread_safe: yes (pure function) */
msgpack_err_t msgpack_enc_map(msgpack_enc_t *enc, uint32_t count);

/* ------------------------------------------------------------------ */
/*  Decoder                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    msgpack_type_t type;
    union {
        bool     boolean;
        uint32_t u32;
        int32_t  i32;
        float    f32;
        struct { const uint8_t *ptr; size_t len; } str;
        struct { const uint8_t *ptr; size_t len; } bin;
        uint32_t array_count;    /* number of elements  */
        uint32_t map_count;      /* number of key-value pairs */
    } val;
} msgpack_token_t;

typedef struct {
    const uint8_t *buf;     /* caller-owned input buffer   */
    size_t         len;     /* total input length          */
    size_t         pos;     /* current read position       */
} msgpack_dec_t;

/** Initialize decoder with caller-provided buffer.
 *  @thread_safe: yes (distinct dec instances) */
msgpack_err_t msgpack_dec_init(msgpack_dec_t *dec,
                               const uint8_t *buf, size_t len);

/** Unpack next token. Returns MSGPACK_ERR_SHORT when no more data.
 *  For ARRAY/MAP tokens, caller must unpack child elements sequentially.
 *  @thread_safe: yes (pure function) */
msgpack_err_t msgpack_dec_next(msgpack_dec_t *dec,
                               msgpack_token_t *tok);

#ifdef __cplusplus
}
#endif

#endif /* MSGPACK_H */
