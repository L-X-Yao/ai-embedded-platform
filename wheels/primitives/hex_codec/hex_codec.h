/*
 * hex_codec — hex string encoder/decoder
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Encodes binary data to hex strings (lower/uppercase) and decodes
 * hex strings back to binary.  Complement to base64 for human-readable
 * binary representation.  Common for debug output, MAC addresses,
 * register dumps.
 *
 * Thread safety: all functions are pure / reentrant (no global state).
 */

#ifndef HEX_CODEC_H
#define HEX_CODEC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HEX_OK             =  0,
    HEX_ERR_PARAMS     = -1,
    HEX_ERR_OVERFLOW   = -2,
    HEX_ERR_INVALID    = -3,
    HEX_ERR_ODD_LENGTH = -4,
} hex_err_t;

/**
 * Encode binary data to lowercase hex string.
 *
 * @param data      Input buffer. May be NULL only when data_len == 0.
 * @param data_len  Number of bytes to encode.
 * @param out       Output buffer for NUL-terminated hex string.
 * @param out_size  Size of output buffer (must be >= data_len*2 + 1).
 * @return          HEX_OK on success, HEX_ERR_PARAMS or HEX_ERR_OVERFLOW.
 * @thread_safe: yes (pure function, no state)
 */
hex_err_t hex_encode(const uint8_t *data, uint16_t data_len,
                     char *out, uint16_t out_size);

/**
 * Encode binary data to uppercase hex string.
 *
 * @param data      Input buffer. May be NULL only when data_len == 0.
 * @param data_len  Number of bytes to encode.
 * @param out       Output buffer for NUL-terminated hex string.
 * @param out_size  Size of output buffer (must be >= data_len*2 + 1).
 * @return          HEX_OK on success, HEX_ERR_PARAMS or HEX_ERR_OVERFLOW.
 * @thread_safe: yes (pure function, no state)
 */
hex_err_t hex_encode_upper(const uint8_t *data, uint16_t data_len,
                           char *out, uint16_t out_size);

/**
 * Decode hex string to binary data.
 *
 * Accepts mixed case (a-f, A-F).  hex_len must be even.
 *
 * @param hex       Input hex string (need not be NUL-terminated).
 * @param hex_len   Number of hex characters to decode.
 * @param out       Output buffer for decoded bytes.
 * @param out_len   On input: buffer size.  On output: bytes written.
 * @return          HEX_OK, HEX_ERR_PARAMS, HEX_ERR_ODD_LENGTH,
 *                  HEX_ERR_OVERFLOW, or HEX_ERR_INVALID.
 * @thread_safe: yes (pure function, no state)
 */
hex_err_t hex_decode(const char *hex, uint16_t hex_len,
                     uint8_t *out, uint16_t *out_len);

/**
 * Calculate encoded hex string length (excluding NUL terminator).
 * @thread_safe: yes (pure function)
 */
uint16_t hex_encoded_size(uint16_t data_len);

/**
 * Calculate decoded binary length from hex string length.
 * @thread_safe: yes (pure function)
 */
uint16_t hex_decoded_size(uint16_t hex_len);

/**
 * Convert a nibble (0-15) to its hex character.
 *
 * @param nibble     Value 0-15.  Values >15 are masked to low 4 bits.
 * @param uppercase  true for 'A'-'F', false for 'a'-'f'.
 * @return           Hex character.
 * @thread_safe: yes (pure function)
 */
char hex_nibble_to_char(uint8_t nibble, bool uppercase);

/**
 * Convert a hex character to its nibble value.
 *
 * @param c  Hex character ('0'-'9', 'a'-'f', 'A'-'F').
 * @return   0-15 on success, -1 on invalid character.
 * @thread_safe: yes (pure function)
 */
int hex_char_to_nibble(char c);

#ifdef __cplusplus
}
#endif

#endif /* HEX_CODEC_H */
