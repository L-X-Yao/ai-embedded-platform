/*
 * tlv_packet — Type-Length-Value encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Variable-size TLV codec for smart cards, NFC, BLE, embedded protocols.
 * Tag: 1 or 2 bytes (high bit of first byte = 2-byte tag).
 * Length: 1 or 2 bytes (high bit = 2-byte length).
 * Pure C99, no heap, zero-copy decode.
 * Thread safety: all functions are pure / reentrant (no global state).
 */

#ifndef TLV_PACKET_H
#define TLV_PACKET_H

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
    TLV_ERR_FORMAT    = -4,
} tlv_err_t;

typedef struct {
    uint16_t       tag;
    const uint8_t *value;   /* points into input buffer (zero-copy) */
    uint16_t       length;
} tlv_t;

/* ---------- Encoder ---------- */

typedef struct {
    uint8_t *buf;
    uint16_t capacity;
    uint16_t pos;
} tlv_encoder_t;

/** Initialize encoder with caller-owned buffer. @thread_safe: yes */
void      tlv_encoder_init(tlv_encoder_t *enc, uint8_t *buf, uint16_t capacity);

/** Encode a raw TLV entry. @thread_safe: yes */
tlv_err_t tlv_encode(tlv_encoder_t *enc, uint16_t tag,
                     const uint8_t *value, uint16_t length);

/** Encode a uint8 value. @thread_safe: yes */
tlv_err_t tlv_encode_u8(tlv_encoder_t *enc, uint16_t tag, uint8_t value);

/** Encode a uint16 value (big-endian). @thread_safe: yes */
tlv_err_t tlv_encode_u16(tlv_encoder_t *enc, uint16_t tag, uint16_t value);

/** Encode a uint32 value (big-endian). @thread_safe: yes */
tlv_err_t tlv_encode_u32(tlv_encoder_t *enc, uint16_t tag, uint32_t value);

/** Encode a NUL-terminated string (value excludes NUL). @thread_safe: yes */
tlv_err_t tlv_encode_string(tlv_encoder_t *enc, uint16_t tag, const char *str);

/** Return current encoded length. @thread_safe: yes */
uint16_t  tlv_encoder_len(const tlv_encoder_t *enc);

/* ---------- Decoder (iterative) ---------- */

typedef struct {
    const uint8_t *buf;
    uint16_t       len;
    uint16_t       pos;
} tlv_decoder_t;

/** Initialize decoder over a buffer. @thread_safe: yes */
void      tlv_decoder_init(tlv_decoder_t *dec, const uint8_t *buf, uint16_t len);

/** Decode next TLV entry. Returns TLV_OK on success. @thread_safe: yes */
tlv_err_t tlv_decode_next(tlv_decoder_t *dec, tlv_t *tlv);

/** True when all bytes consumed. @thread_safe: yes */
bool      tlv_decoder_done(const tlv_decoder_t *dec);

/* ---------- Search ---------- */

/** Find first TLV with matching tag. @thread_safe: yes */
tlv_err_t tlv_find(const uint8_t *buf, uint16_t len, uint16_t tag, tlv_t *result);

#ifdef __cplusplus
}
#endif

#endif /* TLV_PACKET_H */
