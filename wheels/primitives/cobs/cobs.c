// SPDX-License-Identifier: MIT
/*
 * cobs — Consistent Overhead Byte Stuffing encoder/decoder
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "cobs.h"
#include "contract.h"
#include <stddef.h>

#define COBS_BLOCK_MAX 0xFFu  /* 255: max distance code value */

uint16_t cobs_max_encoded_size(uint16_t data_len)
{
    /* overhead = 1 leading code byte + 1 extra byte per 254 input bytes.
     * Saturates: the size of a long input does not fit uint16_t. */
    uint32_t size = (uint32_t)data_len + data_len / 254u + 1u;
    return (size > UINT16_MAX) ? UINT16_MAX : (uint16_t)size;
}

cobs_err_t cobs_encode(const uint8_t *data, uint16_t data_len,
                       uint8_t *out, uint16_t *out_len)
{
    if (out == NULL || out_len == NULL) {
        return COBS_ERR_PARAM;
    }
    if (data == NULL && data_len != 0u) {
        return COBS_ERR_PARAM;
    }

    uint16_t capacity = *out_len;
    uint16_t max_size = cobs_max_encoded_size(data_len);

    if (capacity < max_size) {
        return COBS_ERR_OVERFLOW;
    }

    uint32_t write  = 0;   /* output write index */
    uint32_t code_i = 0;   /* index of current code byte in output */
    uint8_t  code   = 1;   /* distance counter (starts at 1) */

    /* reserve first code byte */
    write = 1;

    for (uint16_t i = 0; i < data_len; i++) {
        /* max_size saturates, so a long input can still outgrow capacity. */
        if (write >= capacity) {
            return COBS_ERR_OVERFLOW;
        }
        if (data[i] == 0x00u) {
            /* finish current block: write distance code */
            out[code_i] = code;
            code_i = write;
            write++;
            code   = 1;
        } else {
            out[write] = data[i];
            write++;
            code++;
            if (code == COBS_BLOCK_MAX) {
                /* block full (254 non-zero bytes) — emit code and start new */
                out[code_i] = code;
                code_i = write;
                write++;
                code   = 1;
            }
        }
    }

    /* final code byte */
    if (code_i >= capacity) {
        return COBS_ERR_OVERFLOW;
    }
    out[code_i] = code;
    *out_len = (uint16_t)write;

    ENSURE(write <= max_size);
    return COBS_OK;
}

cobs_err_t cobs_decode(const uint8_t *encoded, uint16_t enc_len,
                       uint8_t *out, uint16_t *out_len)
{
    if (out == NULL || out_len == NULL) {
        return COBS_ERR_PARAM;
    }
    if (encoded == NULL && enc_len != 0u) {
        return COBS_ERR_PARAM;
    }

    uint16_t capacity = *out_len;
    uint16_t write = 0;
    uint16_t i = 0;

    while (i < enc_len) {
        uint8_t code = encoded[i];
        i++;

        if (code == 0x00u) {
            return COBS_ERR_INVALID;
        }

        /* copy (code - 1) data bytes */
        for (uint8_t j = 1; j < code; j++) {
            if (i >= enc_len) {
                return COBS_ERR_INVALID;
            }
            if (encoded[i] == 0x00u) {
                return COBS_ERR_INVALID;
            }
            if (write >= capacity) {
                return COBS_ERR_OVERFLOW;
            }
            out[write] = encoded[i];
            write++;
            i++;
        }

        /* if code < 0xFF and there is more data, emit an implicit zero */
        if (code < COBS_BLOCK_MAX && i < enc_len) {
            if (write >= capacity) {
                return COBS_ERR_OVERFLOW;
            }
            out[write] = 0x00u;
            write++;
        }
    }

    *out_len = write;
    return COBS_OK;
}
