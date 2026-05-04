// SPDX-License-Identifier: MIT
/*
 * base64 — Base64 encoder/decoder for embedded systems
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * RFC 4648 compliant. Standard alphabet + '=' padding.
 * Zero allocation: caller provides all buffers.
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef BASE64_H
#define BASE64_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BASE64_OK           =  0,
    BASE64_ERR_PARAM   = -1,   /* NULL pointer or invalid argument */
    BASE64_ERR_OVERFLOW = -2,   /* output buffer too small          */
    BASE64_ERR_INVALID  = -3,   /* invalid Base64 character         */
} base64_err_t;

/* ---------- ACSL formal specifications (Frama-C WP) ---------- */

/**
 * Calculate encoded size (without NUL terminator).
 *
 * @param data_len  Number of input bytes.
 * @return          Number of Base64 characters produced.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
/*@ assigns \nothing;
    ensures \result == ((data_len + 2) / 3) * 4;
*/
uint16_t base64_encoded_size(uint16_t data_len);

/**
 * Calculate maximum decoded size (upper bound; ignores padding).
 * May over-estimate by up to 2 bytes. Exact size is known only
 * after inspecting padding characters.
 *
 * @param b64_len   Number of Base64 characters (including padding).
 * @return          Maximum number of decoded bytes.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
/*@ assigns \nothing;
    ensures \result == (b64_len / 4) * 3;
*/
uint16_t base64_decoded_size(uint16_t b64_len);

/**
 * Encode binary data to Base64 string.
 *
 * The output is NUL-terminated. *out_len is set to the number of
 * Base64 characters written (excluding NUL).
 *
 * @param data      Input binary data. May be NULL only when data_len == 0.
 * @param data_len  Number of input bytes.
 * @param out       Output buffer for Base64 string. Must not be NULL.
 * @param out_len   [in] buffer capacity, [out] chars written (excl. NUL).
 * @return          BASE64_OK on success, error code otherwise.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
/*@ requires \valid(out_len);
    requires out != \null;
    requires data_len == 0 || \valid_read(data + (0 .. data_len - 1));
    assigns out[0 .. \at(*out_len, Pre) - 1], *out_len;
    ensures \result == BASE64_OK || \result == BASE64_ERR_PARAM
         || \result == BASE64_ERR_OVERFLOW;
*/
base64_err_t base64_encode(const uint8_t *data, uint16_t data_len,
                           char *out, uint16_t *out_len);

/**
 * Decode Base64 string to binary data.
 *
 * Handles both padded and unpadded input.
 *
 * @param b64       Input Base64 string. May be NULL only when b64_len == 0.
 * @param b64_len   Number of Base64 characters (excluding any NUL).
 * @param out       Output buffer for decoded bytes. Must not be NULL.
 * @param out_len   [in] buffer capacity, [out] bytes written.
 * @return          BASE64_OK on success, error code otherwise.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
/*@ requires \valid(out_len);
    requires out != \null;
    requires b64_len == 0 || \valid_read(b64 + (0 .. b64_len - 1));
    assigns out[0 .. \at(*out_len, Pre) - 1], *out_len;
    ensures \result == BASE64_OK || \result == BASE64_ERR_PARAM
         || \result == BASE64_ERR_OVERFLOW || \result == BASE64_ERR_INVALID;
*/
base64_err_t base64_decode(const char *b64, uint16_t b64_len,
                           uint8_t *out, uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* BASE64_H */
