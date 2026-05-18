// SPDX-License-Identifier: MIT
/*
 * cobs — Consistent Overhead Byte Stuffing encoder/decoder
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Eliminates 0x00 from payload, enabling reliable frame delimiting
 * on serial links. Overhead: 1 byte per 254 bytes of input (worst case).
 * Thread safety: all functions are pure / reentrant.
 */

#ifndef COBS_H
#define COBS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COBS_OK           =  0,
    COBS_ERR_PARAM   = -1,
    COBS_ERR_OVERFLOW = -2,
    COBS_ERR_INVALID  = -3,
} cobs_err_t;

/**
 * Encode data with COBS. Output contains no 0x00 bytes.
 * Caller uses 0x00 as frame delimiter (not included in output).
 *
 * @param data      Input buffer. NULL only valid when data_len == 0.
 * @param data_len  Input length in bytes.
 * @param out       Output buffer. Must be at least cobs_max_encoded_size(data_len).
 * @param out_len   On input: buffer capacity. On output: bytes written.
 * @return          COBS_OK on success, error code otherwise.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
cobs_err_t cobs_encode(const uint8_t *data, uint16_t data_len,
                       uint8_t *out, uint16_t *out_len);

/**
 * Decode COBS-encoded data back to original payload.
 *
 * @param encoded   COBS-encoded input. NULL only valid when enc_len == 0.
 * @param enc_len   Encoded input length in bytes.
 * @param out       Output buffer for decoded data.
 * @param out_len   On input: buffer capacity. On output: bytes written.
 * @return          COBS_OK on success, error code otherwise.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
cobs_err_t cobs_decode(const uint8_t *encoded, uint16_t enc_len,
                       uint8_t *out, uint16_t *out_len);

/**
 * Calculate maximum encoded size for a given input length.
 * Worst case: input has no zeros, overhead = ceil(data_len / 254) + 1.
 *
 * @param data_len  Input length in bytes.
 * @return          Maximum encoded output size.
 * @thread_safe: yes (pure function, no state) @isr_safe: yes
 */
uint16_t cobs_max_encoded_size(uint16_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* COBS_H */
