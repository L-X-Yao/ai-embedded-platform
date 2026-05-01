/*
 * tlv_codec — Tag-Length-Value encoder/decoder for embedded protocols
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Fixed-width TLV format: tag (2 bytes, big-endian) + length (2 bytes,
 * big-endian) + value (N bytes). Iterative decoder with zero-copy value
 * pointers. Typed encode helpers for u8/u16/u32/string.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef TLV_CODEC_H
#define TLV_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TLV_OK            =  0,
    TLV_ERR_PARAMS    = -1,
    TLV_ERR_OVERFLOW  = -2,
    TLV_ERR_TRUNCATED = -3,
    TLV_ERR_INVALID   = -4,
} tlv_err_t;

/** Decoded TLV element. value points into the decode buffer (zero-copy). */
typedef struct {
    uint16_t       tag;
    uint16_t       length;
    const uint8_t *value;
} tlv_t;

/* ── Encoder ─────────────────────────────────────────────────────── */

typedef struct {
    uint8_t *buf;
    uint16_t capacity;
    uint16_t pos;
} tlv_encoder_t;

/** Initialize encoder with caller-provided buffer. */
void       tlv_encoder_init(tlv_encoder_t *enc, uint8_t *buf, uint16_t capacity);

/** Encode a raw TLV element. @thread_safe: yes (no shared state) */
tlv_err_t  tlv_encode(tlv_encoder_t *enc, uint16_t tag,
                      const uint8_t *value, uint16_t length);

/** Encode a uint8 value. */
tlv_err_t  tlv_encode_u8(tlv_encoder_t *enc, uint16_t tag, uint8_t value);

/** Encode a uint16 value (big-endian). */
tlv_err_t  tlv_encode_u16(tlv_encoder_t *enc, uint16_t tag, uint16_t value);

/** Encode a uint32 value (big-endian). */
tlv_err_t  tlv_encode_u32(tlv_encoder_t *enc, uint16_t tag, uint32_t value);

/** Encode a NUL-terminated string (value = string bytes, no NUL). */
tlv_err_t  tlv_encode_string(tlv_encoder_t *enc, uint16_t tag, const char *str);

/** Return number of bytes written so far. */
uint16_t   tlv_encoder_len(const tlv_encoder_t *enc);

/* ── Decoder (iterative) ─────────────────────────────────────────── */

typedef struct {
    const uint8_t *buf;
    uint16_t       len;
    uint16_t       pos;
} tlv_decoder_t;

/** Initialize decoder with a buffer containing one or more TLV elements. */
void       tlv_decoder_init(tlv_decoder_t *dec, const uint8_t *buf, uint16_t len);

/** Decode the next TLV element. Returns TLV_ERR_TRUNCATED at end or on
 *  incomplete data. @thread_safe: yes (no shared state) */
tlv_err_t  tlv_decode_next(tlv_decoder_t *dec, tlv_t *tlv);

/** Return true when all bytes have been consumed. */
bool       tlv_decoder_done(const tlv_decoder_t *dec);

/* ── Value extraction helpers ────────────────────────────────────── */

/** Extract uint8 from a decoded TLV (returns 0 if length < 1). */
uint8_t    tlv_get_u8(const tlv_t *tlv);

/** Extract uint16 from a decoded TLV (big-endian, returns 0 if length < 2). */
uint16_t   tlv_get_u16(const tlv_t *tlv);

/** Extract uint32 from a decoded TLV (big-endian, returns 0 if length < 4). */
uint32_t   tlv_get_u32(const tlv_t *tlv);

#ifdef __cplusplus
}
#endif

#endif /* TLV_CODEC_H */
