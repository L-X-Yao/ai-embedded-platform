// SPDX-License-Identifier: MIT
/*
 * protobuf_lite — lightweight Protocol Buffers wire format encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:    (none)
 *
 * Manual varint/zigzag/length-delimited encoding without .proto compiler.
 * No schema, no code generation — caller knows field numbers.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef PROTOBUF_LITE_H
#define PROTOBUF_LITE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PB_OK           =  0,
    PB_ERR_PARAM   = -1,
    PB_ERR_OVERFLOW = -2,
    PB_ERR_MALFORMED = -3,
    PB_ERR_TRUNCATED = -4,
} pb_err_t;

typedef enum {
    PB_WIRE_VARINT  = 0,
    PB_WIRE_64BIT   = 1,
    PB_WIRE_LENGTH  = 2,
    PB_WIRE_32BIT   = 5,
} pb_wire_type_t;

/* ---- Encoder ---- */

typedef struct {
    uint8_t  *buf;
    uint16_t  capacity;
    uint16_t  pos;
} pb_encoder_t;

/** Initialize encoder with caller-provided buffer. @thread_safe: yes (no shared state) */
void pb_encoder_init(pb_encoder_t *enc, uint8_t *buf, uint16_t capacity);

/** Encode a varint field. @thread_safe: yes (pure function) */
pb_err_t pb_encode_varint(pb_encoder_t *enc, uint8_t field_num, uint64_t value);

/** Encode a signed 32-bit integer using ZigZag encoding. @thread_safe: yes (pure function) */
pb_err_t pb_encode_sint32(pb_encoder_t *enc, uint8_t field_num, int32_t value);

/** Encode a signed 64-bit integer using ZigZag encoding. @thread_safe: yes (pure function) */
pb_err_t pb_encode_sint64(pb_encoder_t *enc, uint8_t field_num, int64_t value);

/** Encode a fixed 32-bit value (little-endian). @thread_safe: yes (pure function) */
pb_err_t pb_encode_fixed32(pb_encoder_t *enc, uint8_t field_num, uint32_t value);

/** Encode a fixed 64-bit value (little-endian). @thread_safe: yes (pure function) */
pb_err_t pb_encode_fixed64(pb_encoder_t *enc, uint8_t field_num, uint64_t value);

/** Encode a float as fixed32 (IEEE 754). @thread_safe: yes (pure function) */
pb_err_t pb_encode_float(pb_encoder_t *enc, uint8_t field_num, float value);

/** Encode a length-delimited byte array. @thread_safe: yes (pure function) */
pb_err_t pb_encode_bytes(pb_encoder_t *enc, uint8_t field_num,
                         const uint8_t *data, uint16_t len);

/** Encode a null-terminated string. @thread_safe: yes (pure function) */
pb_err_t pb_encode_string(pb_encoder_t *enc, uint8_t field_num, const char *str);

/** Return current encoded length. @thread_safe: yes (pure function) */
uint16_t pb_encoder_len(const pb_encoder_t *enc);

/* ---- Decoder ---- */

typedef struct {
    const uint8_t *buf;
    uint16_t       len;
    uint16_t       pos;
} pb_decoder_t;

typedef struct {
    uint8_t        field_num;
    pb_wire_type_t wire_type;
    union {
        uint64_t varint;
        uint32_t fixed32;
        uint64_t fixed64;
        struct {
            const uint8_t *data;
            uint16_t       len;
        } bytes;
    } value;
} pb_field_t;

/** Initialize decoder with input buffer. @thread_safe: yes (no shared state) */
void pb_decoder_init(pb_decoder_t *dec, const uint8_t *buf, uint16_t len);

/** Decode the next field from the buffer. @thread_safe: yes (pure function) */
pb_err_t pb_decode_field(pb_decoder_t *dec, pb_field_t *field);

/** Check if all bytes have been consumed. @thread_safe: yes (pure function) */
bool pb_decoder_done(const pb_decoder_t *dec);

/** Decode a ZigZag-encoded 32-bit signed integer. @thread_safe: yes (pure function) */
int32_t pb_zigzag_decode32(uint32_t n);

/** Decode a ZigZag-encoded 64-bit signed integer. @thread_safe: yes (pure function) */
int64_t pb_zigzag_decode64(uint64_t n);

#ifdef __cplusplus
}
#endif

#endif /* PROTOBUF_LITE_H */
