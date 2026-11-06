// SPDX-License-Identifier: MIT
/*
 * heatshrink -- lightweight LZSS compression/decompression
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Streaming LZSS encoder/decoder with configurable sliding window.
 * All memory is caller-provided.  No malloc, no globals.
 *
 * Bit-level format (MSB-first):
 *   Literal:   flag=1, then 8-bit byte
 *   Back-ref:  flag=0, then window_bits offset, then lookahead_bits length
 *   Length is encoded as (actual_length - 1), minimum actual match = 2.
 */

#include "heatshrink.h"
#include <string.h>

/* ---- helpers ---- */

static uint8_t is_power_of_2(uint16_t v)
{
    return (v != 0) && ((v & (uint16_t)(v - 1u)) == 0) ? 1 : 0;
}

static uint8_t log2_u16(uint16_t v)
{
    uint8_t r = 0;
    while (v > 1u) {
        v >>= 1;
        r++;
    }
    return r;
}

/* ================================================================
 *  ENCODER
 * ================================================================ */

heatshrink_err_t heatshrink_encoder_init(heatshrink_encoder_t *enc,
                                         uint8_t *window_buf,
                                         uint16_t window_size,
                                         uint8_t lookahead_bits)
{
    if (!enc || !window_buf) {
        return HEATSHRINK_ERR_PARAM;
    }
    if (!is_power_of_2(window_size) || window_size < 256u || window_size > 2048u) {
        return HEATSHRINK_ERR_PARAM;
    }
    if (lookahead_bits < 4 || lookahead_bits > 8) {
        return HEATSHRINK_ERR_PARAM;
    }

    uint8_t wbits = log2_u16(window_size);
    if (lookahead_bits >= wbits) {
        return HEATSHRINK_ERR_PARAM;
    }

    memset(enc, 0, sizeof(*enc));
    enc->window        = window_buf;
    enc->window_size   = window_size;
    enc->window_bits   = wbits;
    enc->lookahead_bits = lookahead_bits;
    enc->state         = HS_ENC_READY;

    memset(window_buf, 0, window_size);
    return HEATSHRINK_OK;
}

heatshrink_err_t heatshrink_encoder_reset(heatshrink_encoder_t *enc)
{
    if (!enc) {
        return HEATSHRINK_ERR_PARAM;
    }
    uint8_t *win = enc->window;
    uint16_t ws  = enc->window_size;
    uint8_t  wb  = enc->window_bits;
    uint8_t  lb  = enc->lookahead_bits;

    memset(enc, 0, sizeof(*enc));
    enc->window         = win;
    enc->window_size    = ws;
    enc->window_bits    = wb;
    enc->lookahead_bits = lb;
    enc->state          = HS_ENC_READY;

    memset(win, 0, ws);
    return HEATSHRINK_OK;
}

heatshrink_sink_res_t heatshrink_encoder_sink(heatshrink_encoder_t *enc,
                                              const uint8_t *data,
                                              uint16_t len,
                                              uint16_t *consumed)
{
    if (!enc || !data || !consumed) {
        return (heatshrink_sink_res_t)HEATSHRINK_ERR_PARAM;
    }

    if (enc->state == HS_ENC_DONE) {
        *consumed = 0;
        return HEATSHRINK_SINK_FULL;
    }

    /* How much room is left in the window? */
    uint16_t remaining = enc->window_size - enc->input_pos;
    if (remaining == 0) {
        if (enc->match_scan < enc->input_pos) {
            *consumed = 0;
            return HEATSHRINK_SINK_FULL;
        }
        /* Window fully encoded: start it over.  New data cannot reference the
         * old contents, and the decoder needs no notice of the restart. */
        enc->input_pos = 0;
        enc->match_scan = 0;
        remaining = enc->window_size;
    }

    uint16_t to_copy = (len < remaining) ? len : remaining;
    memcpy(&enc->window[enc->input_pos], data, to_copy);
    enc->input_pos = (uint16_t)(enc->input_pos + to_copy);
    *consumed = to_copy;

    if (enc->state == HS_ENC_READY) {
        enc->state = HS_ENC_FILLING;
    }

    return (to_copy < len) ? HEATSHRINK_SINK_FULL : HEATSHRINK_SINK_OK;
}

/* ---- bit-level output ---- */

/* Write a completed byte to the output.  Returns -1 if the output is full. */
static int enc_flush_byte(heatshrink_encoder_t *enc,
                          uint8_t *out, uint16_t out_size, uint16_t *out_idx)
{
    if (enc->out_bit_idx < 8) {
        return 0;
    }
    if (*out_idx >= out_size) {
        return -1;
    }
    out[*out_idx] = enc->out_bits;
    (*out_idx)++;
    enc->out_bits = 0;
    enc->out_bit_idx = 0;
    return 0;
}

/* Move the pending token to the output, MSB first.  Returns -1 if the output
 * fills up; the bits left in the token wait for the next poll. */
static int enc_emit_token(heatshrink_encoder_t *enc,
                          uint8_t *out, uint16_t out_size, uint16_t *out_idx)
{
    while (enc->token_bits > 0) {
        if (enc_flush_byte(enc, out, out_size, out_idx) < 0) {
            return -1;
        }
        enc->token_bits--;
        uint8_t bit = (uint8_t)((enc->token >> enc->token_bits) & 1u);
        enc->out_bits |= (uint8_t)(bit << (7u - enc->out_bit_idx));
        enc->out_bit_idx++;
    }
    return enc_flush_byte(enc, out, out_size, out_idx);
}

/* Find the longest match looking back from scan_pos. */
static void find_longest_match(const uint8_t *window,
                               uint16_t scan_pos,
                               uint16_t input_end,
                               uint16_t max_back,
                               uint16_t max_match,
                               uint16_t *best_offset,
                               uint16_t *best_len)
{
    *best_len = 0;
    *best_offset = 0;

    if (scan_pos >= input_end) {
        return;
    }

    uint16_t avail = (uint16_t)(input_end - scan_pos);
    uint16_t max_len = (avail < max_match) ? avail : max_match;

    uint16_t search_start = (scan_pos > max_back)
                            ? (uint16_t)(scan_pos - max_back) : 0;

    for (uint16_t i = search_start; i < scan_pos; i++) {
        uint16_t mlen = 0;
        while (mlen < max_len &&
               window[i + mlen] == window[scan_pos + mlen]) {
            mlen++;
        }
        if (mlen > *best_len) {
            *best_len = mlen;
            *best_offset = (uint16_t)(scan_pos - i);
            if (mlen == max_len) {
                break;
            }
        }
    }
}

heatshrink_poll_res_t heatshrink_encoder_poll(heatshrink_encoder_t *enc,
                                              uint8_t *out,
                                              uint16_t out_size,
                                              uint16_t *produced)
{
    if (!enc || !out || !produced) {
        return (heatshrink_poll_res_t)HEATSHRINK_ERR_PARAM;
    }

    *produced = 0;

    if (enc->state == HS_ENC_READY || enc->state == HS_ENC_DONE) {
        return HEATSHRINK_POLL_EMPTY;
    }

    uint16_t max_match = (uint16_t)(1u << enc->lookahead_bits);
    uint16_t max_back = (uint16_t)(1u << enc->window_bits);
    uint16_t out_idx = 0;

    for (;;) {
        if (enc_emit_token(enc, out, out_size, &out_idx) < 0) {
            *produced = out_idx;
            enc->state = HS_ENC_YIELDING;
            return HEATSHRINK_POLL_MORE;
        }
        if (enc->match_scan >= enc->input_pos) {
            break;
        }

        uint16_t best_len = 0;
        uint16_t best_offset = 0;
        find_longest_match(enc->window,
                           enc->match_scan, enc->input_pos,
                           max_back, max_match,
                           &best_offset, &best_len);

        if (best_len >= 2) {
            /* Back-reference: flag(0) + offset-1(window_bits)
             * + length-1(lookahead_bits) */
            enc->token = ((uint32_t)(best_offset - 1u) << enc->lookahead_bits) |
                         (uint32_t)(best_len - 1u);
            enc->token_bits = (uint8_t)(1u + enc->window_bits + enc->lookahead_bits);
            enc->match_scan = (uint16_t)(enc->match_scan + best_len);
        } else {
            /* Literal: flag(1) + byte(8) */
            enc->token = 0x100u | enc->window[enc->match_scan];
            enc->token_bits = 9;
            enc->match_scan++;
        }
    }

    /* All buffered input encoded */
    *produced = out_idx;

    if (enc->finishing) {
        /* Flush partial byte */
        if (enc->out_bit_idx > 0) {
            if (out_idx < out_size) {
                out[out_idx] = enc->out_bits;
                out_idx++;
                *produced = out_idx;
                enc->out_bits = 0;
                enc->out_bit_idx = 0;
            } else {
                enc->state = HS_ENC_FINISHING;
                return HEATSHRINK_POLL_MORE;
            }
        }
        enc->state = HS_ENC_DONE;
        return HEATSHRINK_POLL_EMPTY;
    }

    enc->state = HS_ENC_READY;
    return HEATSHRINK_POLL_EMPTY;
}

heatshrink_finish_res_t heatshrink_encoder_finish(heatshrink_encoder_t *enc)
{
    if (!enc) {
        return (heatshrink_finish_res_t)HEATSHRINK_ERR_PARAM;
    }

    enc->finishing = 1;

    if (enc->state == HS_ENC_DONE) {
        return HEATSHRINK_FINISH_DONE;
    }

    if (enc->match_scan >= enc->input_pos && enc->token_bits == 0 &&
        enc->out_bit_idx == 0) {
        enc->state = HS_ENC_DONE;
        return HEATSHRINK_FINISH_DONE;
    }

    if (enc->state == HS_ENC_READY) {
        enc->state = HS_ENC_FILLING;
    }

    return HEATSHRINK_FINISH_MORE;
}

/* ================================================================
 *  DECODER
 * ================================================================ */

heatshrink_err_t heatshrink_decoder_init(heatshrink_decoder_t *dec,
                                         uint8_t *window_buf,
                                         uint16_t window_size,
                                         uint8_t lookahead_bits)
{
    if (!dec || !window_buf) {
        return HEATSHRINK_ERR_PARAM;
    }
    if (!is_power_of_2(window_size) || window_size < 256u || window_size > 2048u) {
        return HEATSHRINK_ERR_PARAM;
    }
    if (lookahead_bits < 4 || lookahead_bits > 8) {
        return HEATSHRINK_ERR_PARAM;
    }

    uint8_t wbits = log2_u16(window_size);
    if (lookahead_bits >= wbits) {
        return HEATSHRINK_ERR_PARAM;
    }

    memset(dec, 0, sizeof(*dec));
    dec->window         = window_buf;
    dec->window_size    = window_size;
    dec->window_bits    = wbits;
    dec->lookahead_bits = lookahead_bits;
    dec->state          = HS_DEC_READY;

    /* Input buffer is in the second half of the provided buffer */
    dec->input_buf  = &window_buf[window_size];
    dec->input_size = 0;
    dec->input_index = 0;

    memset(window_buf, 0, window_size);
    return HEATSHRINK_OK;
}

heatshrink_err_t heatshrink_decoder_reset(heatshrink_decoder_t *dec)
{
    if (!dec) {
        return HEATSHRINK_ERR_PARAM;
    }
    uint8_t *win = dec->window;
    uint16_t ws  = dec->window_size;
    uint8_t  wb  = dec->window_bits;
    uint8_t  lb  = dec->lookahead_bits;

    uint8_t *ibuf = dec->input_buf;

    memset(dec, 0, sizeof(*dec));
    dec->window         = win;
    dec->window_size    = ws;
    dec->window_bits    = wb;
    dec->lookahead_bits = lb;
    dec->state          = HS_DEC_READY;
    dec->input_buf      = ibuf;
    dec->input_size     = 0;
    dec->input_index    = 0;

    memset(win, 0, ws);
    return HEATSHRINK_OK;
}

heatshrink_sink_res_t heatshrink_decoder_sink(heatshrink_decoder_t *dec,
                                              const uint8_t *data,
                                              uint16_t len,
                                              uint16_t *consumed)
{
    if (!dec || !data || !consumed) {
        return (heatshrink_sink_res_t)HEATSHRINK_ERR_PARAM;
    }

    if (dec->state == HS_DEC_DONE) {
        *consumed = 0;
        return HEATSHRINK_SINK_FULL;
    }

    /* Input buffer capacity = window_size / 2 */
    uint16_t ibuf_cap = (uint16_t)(dec->window_size / 2u);
    uint16_t remaining = (uint16_t)(ibuf_cap - dec->input_size);
    if (remaining == 0) {
        *consumed = 0;
        return HEATSHRINK_SINK_FULL;
    }

    uint16_t to_copy = (len < remaining) ? len : remaining;
    memcpy(&dec->input_buf[dec->input_size], data, to_copy);
    dec->input_size = (uint16_t)(dec->input_size + to_copy);
    *consumed = to_copy;

    if (dec->state == HS_DEC_READY) {
        dec->state = HS_DEC_TAG;
    }

    return (to_copy < len) ? HEATSHRINK_SINK_FULL : HEATSHRINK_SINK_OK;
}

/* ---- bit-level input helpers ---- */

/* Try to get nbits from the input stream.  Returns -1 if insufficient. */
static int dec_get_bits(heatshrink_decoder_t *dec, uint8_t nbits,
                        uint16_t *out)
{
    /* Check if we have enough bits before consuming any */
    uint8_t  save_bit_idx   = dec->bit_idx;
    uint16_t save_input_idx = dec->input_index;
    uint16_t save_cur_byte  = dec->current_byte;

    uint16_t result = 0;
    for (uint8_t i = 0; i < nbits; i++) {
        if (dec->bit_idx == 0) {
            if (dec->input_index >= dec->input_size) {
                /* Restore state -- we didn't have enough bits */
                dec->bit_idx      = save_bit_idx;
                dec->input_index  = save_input_idx;
                dec->current_byte = save_cur_byte;
                return -1;
            }
            dec->current_byte = dec->input_buf[dec->input_index];
            dec->input_index++;
            dec->bit_idx = 8;
        }
        dec->bit_idx--;
        result = (uint16_t)((result << 1u) |
                             (uint16_t)((dec->current_byte >> dec->bit_idx) & 1u));
    }
    *out = result;
    return 0;
}

heatshrink_poll_res_t heatshrink_decoder_poll(heatshrink_decoder_t *dec,
                                              uint8_t *out,
                                              uint16_t out_size,
                                              uint16_t *produced)
{
    if (!dec || !out || !produced) {
        return (heatshrink_poll_res_t)HEATSHRINK_ERR_PARAM;
    }

    *produced = 0;

    if (dec->state == HS_DEC_READY || dec->state == HS_DEC_DONE) {
        return HEATSHRINK_POLL_EMPTY;
    }

    uint16_t out_idx = 0;
    uint16_t wmask = (uint16_t)(dec->window_size - 1u);

    /* Yield pending output first */
    while (dec->state == HS_DEC_YIELD_LITERAL ||
           dec->state == HS_DEC_YIELD_BACKREF) {
        if (dec->output_count > 0) {
            if (out_idx >= out_size) {
                *produced = out_idx;
                return HEATSHRINK_POLL_MORE;
            }

            if (dec->state == HS_DEC_YIELD_LITERAL) {
                out[out_idx] = dec->window[dec->output_index & wmask];
                out_idx++;
                dec->output_count--;
                dec->output_index++;
            } else {
                uint8_t byte = dec->window[dec->match_pos & wmask];
                dec->window[dec->head_index & wmask] = byte;
                out[out_idx] = byte;
                out_idx++;
                dec->head_index++;
                dec->match_pos++;
                dec->output_count--;
            }
        }

        if (dec->output_count == 0) {
            dec->state = HS_DEC_TAG;
        }
    }

    /* Process input bits */
    while (dec->state == HS_DEC_TAG ||
           dec->state == HS_DEC_LITERAL ||
           dec->state == HS_DEC_BACKREF_INDEX ||
           dec->state == HS_DEC_BACKREF_COUNT) {

        if (dec->state == HS_DEC_TAG) {
            uint16_t tag = 0;
            if (dec_get_bits(dec, 1, &tag) < 0) {
                goto need_input;
            }
            dec->state = (tag == 1) ? HS_DEC_LITERAL : HS_DEC_BACKREF_INDEX;
        }

        if (dec->state == HS_DEC_LITERAL) {
            uint16_t byte_val = 0;
            if (dec_get_bits(dec, 8, &byte_val) < 0) {
                goto need_input;
            }
            dec->window[dec->head_index & wmask] = (uint8_t)byte_val;
            dec->output_index = dec->head_index;
            dec->head_index++;
            dec->output_count = 1;
            dec->state = HS_DEC_YIELD_LITERAL;

            if (out_idx >= out_size) {
                *produced = out_idx;
                return HEATSHRINK_POLL_MORE;
            }
            out[out_idx] = (uint8_t)byte_val;
            out_idx++;
            dec->output_count = 0;
            dec->state = HS_DEC_TAG;
            continue;
        }

        if (dec->state == HS_DEC_BACKREF_INDEX) {
            uint16_t idx = 0;
            if (dec_get_bits(dec, dec->window_bits, &idx) < 0) {
                goto need_input;
            }
            dec->match_pos = (uint16_t)(dec->head_index - idx - 1u);
            dec->state = HS_DEC_BACKREF_COUNT;
        }

        if (dec->state == HS_DEC_BACKREF_COUNT) {
            uint16_t cnt = 0;
            if (dec_get_bits(dec, dec->lookahead_bits, &cnt) < 0) {
                goto need_input;
            }
            dec->match_len = (uint16_t)(cnt + 1u);
            dec->output_count = dec->match_len;
            dec->state = HS_DEC_YIELD_BACKREF;

            while (dec->output_count > 0) {
                if (out_idx >= out_size) {
                    *produced = out_idx;
                    return HEATSHRINK_POLL_MORE;
                }
                uint8_t byte = dec->window[dec->match_pos & wmask];
                dec->window[dec->head_index & wmask] = byte;
                out[out_idx] = byte;
                out_idx++;
                dec->head_index++;
                dec->match_pos++;
                dec->output_count--;
            }
            dec->state = HS_DEC_TAG;
            continue;
        }
    }

need_input:
    /* Compact consumed input */
    if (dec->input_index > 0) {
        uint16_t left = (uint16_t)(dec->input_size - dec->input_index);
        if (left > 0) {
            memmove(dec->input_buf, &dec->input_buf[dec->input_index], left);
        }
        dec->input_size = left;
        dec->input_index = 0;
    }

    *produced = out_idx;

    /* Every state that gets here ran out of bits.  Once finishing, none
     * are coming: what is left is the encoder's zero padding, whose first
     * bit reads as a back-reference tag, or a truncated token. */
    if (dec->finishing) {
        dec->state = HS_DEC_DONE;
    }

    if (out_idx > 0) {
        return HEATSHRINK_POLL_MORE;
    }
    return HEATSHRINK_POLL_EMPTY;
}

/* Bits the current state needs before it can produce output.  0 for the
 * yield states, which already have output pending. */
static uint16_t dec_bits_needed(const heatshrink_decoder_t *dec)
{
    switch (dec->state) {
    case HS_DEC_TAG:
        return 9u;  /* 1 (tag) + 8 (literal), the shortest token */
    case HS_DEC_LITERAL:
        return 8u;
    case HS_DEC_BACKREF_INDEX:
        return (uint16_t)(dec->window_bits + dec->lookahead_bits);
    case HS_DEC_BACKREF_COUNT:
        return dec->lookahead_bits;
    default:
        return 0u;
    }
}

heatshrink_finish_res_t heatshrink_decoder_finish(heatshrink_decoder_t *dec)
{
    if (!dec) {
        return (heatshrink_finish_res_t)HEATSHRINK_ERR_PARAM;
    }

    dec->finishing = 1;

    if (dec->state == HS_DEC_DONE) {
        return HEATSHRINK_FINISH_DONE;
    }

    /* No stream at all, or too few bits left to complete a token: the
     * rest is padding or a truncated token, so poll() has nothing to do. */
    uint16_t bits_avail = (uint16_t)((uint16_t)(dec->input_size - dec->input_index) * 8u
                                     + dec->bit_idx);
    if (dec->state == HS_DEC_READY || bits_avail < dec_bits_needed(dec)) {
        dec->state = HS_DEC_DONE;
        return HEATSHRINK_FINISH_DONE;
    }

    return HEATSHRINK_FINISH_MORE;
}
