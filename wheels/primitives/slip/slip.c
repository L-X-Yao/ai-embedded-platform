// SPDX-License-Identifier: MIT
/*
 * slip — SLIP (Serial Line Internet Protocol) framing encoder/decoder
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "slip.h"
#include <stddef.h>

/* ── block API ──────────────────────────────────────────────────────── */

uint16_t slip_max_encoded_size(uint16_t data_len)
{
    /* worst case: every byte is END or ESC (each expands to 2 bytes)
     * plus 2 delimiter ENDs (leading + trailing).
     * Guard against uint16_t overflow for large data_len values. */
    if (data_len > 32766u) { return UINT16_MAX; }
    return (uint16_t)(data_len * 2u + 2u);
}

slip_err_t slip_encode(const uint8_t *data, uint16_t data_len,
                       uint8_t *out, uint16_t *out_len)
{
    if (out == NULL || out_len == NULL) {
        return SLIP_ERR_PARAM;
    }
    if (data == NULL && data_len != 0u) {
        return SLIP_ERR_PARAM;
    }

    uint16_t capacity = *out_len;
    uint16_t max_size = slip_max_encoded_size(data_len);

    if (capacity < max_size) {
        return SLIP_ERR_OVERFLOW;
    }

    uint16_t w = 0;

    /* leading END delimiter */
    out[w++] = (uint8_t)SLIP_END;

    /* encode payload */
    for (uint16_t i = 0; i < data_len; i++) {
        if (data[i] == (uint8_t)SLIP_END) {
            out[w++] = (uint8_t)SLIP_ESC;
            out[w++] = (uint8_t)SLIP_ESC_END;
        } else if (data[i] == (uint8_t)SLIP_ESC) {
            out[w++] = (uint8_t)SLIP_ESC;
            out[w++] = (uint8_t)SLIP_ESC_ESC;
        } else {
            out[w++] = data[i];
        }
    }

    /* trailing END delimiter */
    out[w++] = (uint8_t)SLIP_END;

    *out_len = w;
    return SLIP_OK;
}

slip_err_t slip_decode(const uint8_t *encoded, uint16_t enc_len,
                       uint8_t *out, uint16_t *out_len)
{
    if (out == NULL || out_len == NULL) {
        return SLIP_ERR_PARAM;
    }
    if (encoded == NULL && enc_len != 0u) {
        return SLIP_ERR_PARAM;
    }

    uint16_t capacity = *out_len;
    uint16_t w = 0;
    uint16_t i = 0;

    /* skip leading END bytes */
    while (i < enc_len && encoded[i] == (uint8_t)SLIP_END) {
        i++;
    }

    bool escaped = false;

    for (; i < enc_len; i++) {
        uint8_t b = encoded[i];

        if (b == (uint8_t)SLIP_END) {
            /* frame delimiter — end of frame */
            break;
        }

        if (escaped) {
            escaped = false;
            if (b == (uint8_t)SLIP_ESC_END) {
                b = (uint8_t)SLIP_END;
            } else if (b == (uint8_t)SLIP_ESC_ESC) {
                b = (uint8_t)SLIP_ESC;
            } else {
                return SLIP_ERR_INVALID;
            }
        } else if (b == (uint8_t)SLIP_ESC) {
            escaped = true;
            continue;
        }

        if (w >= capacity) {
            return SLIP_ERR_OVERFLOW;
        }
        out[w++] = b;
    }

    /* ESC at end of frame with nothing following is invalid */
    if (escaped) {
        return SLIP_ERR_INVALID;
    }

    *out_len = w;
    return SLIP_OK;
}

/* ── streaming decoder ──────────────────────────────────────────────── */

slip_err_t slip_decoder_init(slip_decoder_t *dec, uint8_t *buf, uint16_t capacity)
{
    if (dec == NULL || buf == NULL) {
        return SLIP_ERR_PARAM;
    }

    dec->buf         = buf;
    dec->capacity    = capacity;
    dec->pos         = 0;
    dec->escaped     = false;
    dec->frame_ready = false;

    return SLIP_OK;
}

slip_err_t slip_decoder_feed(slip_decoder_t *dec, uint8_t byte)
{
    if (dec == NULL) {
        return SLIP_ERR_PARAM;
    }

    /* if a frame is already ready, ignore until reset */
    if (dec->frame_ready) {
        return SLIP_OK;
    }

    if (byte == (uint8_t)SLIP_END) {
        if (dec->pos > 0u || dec->escaped) {
            /* end of frame (only if we have data) */
            if (dec->escaped) {
                /* ESC immediately followed by END is invalid */
                dec->escaped = false;
                dec->pos     = 0;
                return SLIP_ERR_INVALID;
            }
            dec->frame_ready = true;
        }
        /* leading ENDs (pos == 0) are silently consumed */
        return SLIP_OK;
    }

    if (dec->escaped) {
        dec->escaped = false;
        uint8_t decoded;
        if (byte == (uint8_t)SLIP_ESC_END) {
            decoded = (uint8_t)SLIP_END;
        } else if (byte == (uint8_t)SLIP_ESC_ESC) {
            decoded = (uint8_t)SLIP_ESC;
        } else {
            /* invalid escape sequence — reset */
            dec->pos = 0;
            return SLIP_ERR_INVALID;
        }

        if (dec->pos >= dec->capacity) {
            dec->pos = 0;
            return SLIP_ERR_OVERFLOW;
        }
        dec->buf[dec->pos++] = decoded;
        return SLIP_OK;
    }

    if (byte == (uint8_t)SLIP_ESC) {
        dec->escaped = true;
        return SLIP_OK;
    }

    /* regular data byte */
    if (dec->pos >= dec->capacity) {
        dec->pos = 0;
        return SLIP_ERR_OVERFLOW;
    }
    dec->buf[dec->pos++] = byte;
    return SLIP_OK;
}

bool slip_decoder_ready(const slip_decoder_t *dec)
{
    if (dec == NULL) {
        return false;
    }
    return dec->frame_ready;
}

uint16_t slip_decoder_frame(const slip_decoder_t *dec, const uint8_t **data)
{
    if (dec == NULL || data == NULL || !dec->frame_ready) {
        if (data != NULL) {
            *data = NULL;
        }
        return 0;
    }
    *data = dec->buf;
    return dec->pos;
}

void slip_decoder_reset(slip_decoder_t *dec)
{
    if (dec == NULL) {
        return;
    }
    dec->pos         = 0;
    dec->escaped     = false;
    dec->frame_ready = false;
}
