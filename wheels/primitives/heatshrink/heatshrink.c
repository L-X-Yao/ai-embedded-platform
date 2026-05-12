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
        *consumed = 0;
        return HEATSHRINK_SINK_FULL;
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

/* Push a single bit to the output accumulator, flushing complete bytes
 * to the output buffer.  Returns 0 on success, -1 if output is full. */
static int enc_push_bit(heatshrink_encoder_t *enc,
                        uint8_t *out, uint16_t out_size, uint16_t *out_idx,
                        uint8_t bit)
{
    enc->out_bits |= (uint8_t)(bit << (7u - enc->out_bit_idx));
    enc->out_bit_idx++;

    if (enc->out_bit_idx == 8) {
        if (*out_idx >= out_size) {
            return -1;  /* output full */
        }
        out[*out_idx] = enc->out_bits;
        (*out_idx)++;
        enc->out_bits = 0;
        enc->out_bit_idx = 0;
    }
    return 0;
}

/* Push nbits of val (MSB-first) to the output.  Returns 0 on success. */
static int enc_push_bits(heatshrink_encoder_t *enc,
                         uint8_t *out, uint16_t out_size, uint16_t *out_idx,
                         uint16_t val, uint8_t nbits)
{
    for (uint8_t i = 0; i < nbits; i++) {
        uint8_t bit = (uint8_t)((val >> (nbits - 1u - i)) & 1u);
        if (enc_push_bit(enc, out, out_size, out_idx, bit) < 0) {
            return -1;
        }
    }
    return 0;
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

    /* Nothing to compress */
    if (enc->match_scan >= enc->input_pos) {
        if (enc->finishing) {
            /* Flush partial byte */
            if (enc->out_bit_idx > 0) {
                if (out_size > 0) {
                    out[0] = enc->out_bits;
                    *produced = 1;
                    enc->out_bits = 0;
                    enc->out_bit_idx = 0;
                }
            }
            enc->state = HS_ENC_DONE;
        } else {
            enc->state = HS_ENC_READY;
        }
        return HEATSHRINK_POLL_EMPTY;
    }

    /* Transition to SEARCHING */
    if (enc->state == HS_ENC_FILLING) {
        enc->state = HS_ENC_SEARCHING;
    }

    uint16_t max_match = (uint16_t)(1u << enc->lookahead_bits);
    uint16_t max_back = (uint16_t)(1u << enc->window_bits);
    uint16_t out_idx = 0;

    while (enc->match_scan < enc->input_pos) {
        uint16_t best_len = 0;
        uint16_t best_offset = 0;
        find_longest_match(enc->window,
                           enc->match_scan, enc->input_pos,
                           max_back, max_match,
                           &best_offset, &best_len);

        if (best_len >= 2) {
            /* Emit back-reference: flag(0) + offset-1(window_bits)
             * + length-1(lookahead_bits) */
            uint16_t coded_offset = (uint16_t)(best_offset - 1u);
            uint16_t coded_len = (uint16_t)(best_len - 1u);

            if (enc_push_bits(enc, out, out_size, &out_idx, 0, 1) < 0 ||
                enc_push_bits(enc, out, out_size, &out_idx,
                              coded_offset, enc->window_bits) < 0 ||
                enc_push_bits(enc, out, out_size, &out_idx,
                              coded_len, enc->lookahead_bits) < 0) {
                /* Output buffer full */
                *produced = out_idx;
                enc->state = HS_ENC_YIELDING;
                return HEATSHRINK_POLL_MORE;
            }
            enc->match_scan = (uint16_t)(enc->match_scan + best_len);
        } else {
            /* Emit literal: flag(1) + byte(8) */
            uint16_t byte_val = enc->window[enc->match_scan];
            if (enc_push_bits(enc, out, out_size, &out_idx, 1, 1) < 0 ||
                enc_push_bits(enc, out, out_size, &out_idx, byte_val, 8) < 0) {
                *produced = out_idx;
                enc->state = HS_ENC_YIELDING;
                return HEATSHRINK_POLL_MORE;
            }
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

    if (enc->match_scan >= enc->input_pos && enc->out_bit_idx == 0) {
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

    return HEATSHRINK_SINK_OK;
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

    /* If finishing and no more complete tokens can be read, we're done.
     * Remaining bits (if any) are zero-padding from the encoder's
     * final partial byte -- not real data. */
    if (dec->finishing && dec->state == HS_DEC_TAG) {
        dec->state = HS_DEC_DONE;
        if (out_idx > 0) {
            return HEATSHRINK_POLL_MORE;
        }
        return HEATSHRINK_POLL_EMPTY;
    }

    if (out_idx > 0) {
        return HEATSHRINK_POLL_MORE;
    }
    return HEATSHRINK_POLL_EMPTY;
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

    /* If we're waiting for a tag and there isn't enough input for any
     * complete token, the remaining bits are padding -- mark done. */
    if (dec->state == HS_DEC_TAG && dec->output_count == 0) {
        /* Try to read a tag + minimum token (1 + 8 bits for literal).
         * If we can't, there's no more real data. */
        uint16_t bits_avail = (uint16_t)((uint16_t)(dec->input_size - dec->input_index) * 8u
                                         + dec->bit_idx);
        if (bits_avail < 9u) {  /* need at least 1 (tag) + 8 (literal) */
            dec->state = HS_DEC_DONE;
            return HEATSHRINK_FINISH_DONE;
        }
    }

    return HEATSHRINK_FINISH_MORE;
}
