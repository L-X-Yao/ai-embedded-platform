// SPDX-License-Identifier: MIT
/*
 * heatshrink -- lightweight LZSS compression/decompression
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * LZSS-based streaming compression using a small sliding window.
 * Designed for embedded: fixed memory, no malloc, configurable
 * window (8-11 bits = 256B-2KB) and lookahead (4-8 bits).
 *
 * Bit-level encoding:
 *   Literal:    1-bit flag (1) + 8-bit byte value
 *   Back-ref:   1-bit flag (0) + window_bits offset + lookahead_bits length
 *
 * All buffers are caller-provided.
 */

#ifndef HEATSHRINK_H
#define HEATSHRINK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Error / result codes ---- */

typedef enum {
    HEATSHRINK_OK        =  0,
    HEATSHRINK_ERR_PARAM = -1,
    HEATSHRINK_ERR_FULL  = -2,
    HEATSHRINK_ERR_EMPTY = -3,
} heatshrink_err_t;

/* Each result type below has a negative member on purpose. arm-none-eabi-gcc
 * sizes an enum by its values, so with only 0 and 1 it would be one unsigned
 * byte and -1 would arrive as 255. All three equal HEATSHRINK_ERR_PARAM. */

typedef enum {
    HEATSHRINK_SINK_ERR_PARAM = -1, /* NULL argument                 */
    HEATSHRINK_SINK_OK   = 0,   /* data accepted                     */
    HEATSHRINK_SINK_FULL = 1,   /* input buffer full, poll first     */
} heatshrink_sink_res_t;

typedef enum {
    HEATSHRINK_POLL_ERR_PARAM = -1, /* NULL argument                 */
    HEATSHRINK_POLL_MORE  = 0,  /* output available, call again      */
    HEATSHRINK_POLL_EMPTY = 1,  /* no more output right now          */
} heatshrink_poll_res_t;

typedef enum {
    HEATSHRINK_FINISH_ERR_PARAM = -1, /* NULL argument               */
    HEATSHRINK_FINISH_MORE = 0, /* more output pending               */
    HEATSHRINK_FINISH_DONE = 1, /* compression/decompression done    */
} heatshrink_finish_res_t;

/* ---- Encoder ---- */

typedef enum {
    HS_ENC_READY      = 0,
    HS_ENC_FILLING    = 1,
    HS_ENC_SEARCHING  = 2,
    HS_ENC_YIELDING   = 3,
    HS_ENC_FINISHING  = 4,
    HS_ENC_DONE       = 5,
} heatshrink_enc_state_t;

typedef struct {
    uint8_t            *window;       /* caller-provided sliding window  */
    uint16_t            window_size;  /* must be power-of-2              */
    uint8_t             window_bits;  /* log2(window_size)               */
    uint8_t             lookahead_bits;

    heatshrink_enc_state_t state;

    uint16_t            input_pos;    /* write cursor into window        */
    uint16_t            match_scan;   /* scan cursor for matching        */

    /* token being written out, kept across a full output buffer */
    uint32_t            token;        /* token bits, right-aligned       */
    uint8_t             token_bits;   /* bits of token still to emit     */

    /* bit-level output accumulator */
    uint8_t             out_bits;     /* pending output bits             */
    uint8_t             out_bit_idx;  /* how many bits in out_bits (0-8) */

    uint8_t             finishing;    /* finish() has been called        */
} heatshrink_encoder_t;

/**
 * Initialise the encoder.
 *
 * @param enc            Caller-allocated encoder.  Must not be NULL.
 * @param window_buf     Buffer for the sliding window.  Must not be NULL.
 * @param window_size    Size in bytes (must be power-of-2, 256..2048).
 * @param lookahead_bits Lookahead size in bits (4..8).
 * @return HEATSHRINK_OK on success, HEATSHRINK_ERR_PARAM on bad args.
 * @thread_safe: no (mutates encoder context)
 * @isr_safe: yes (no locking, no allocation; caller must own the context)
 */
heatshrink_err_t heatshrink_encoder_init(heatshrink_encoder_t *enc,
                                         uint8_t *window_buf,
                                         uint16_t window_size,
                                         uint8_t lookahead_bits);

/**
 * Reset encoder to initial state (reuse same buffers).
 * @thread_safe: no
 */
heatshrink_err_t heatshrink_encoder_reset(heatshrink_encoder_t *enc);

/**
 * Feed input data to the encoder.
 *
 * @param enc      Encoder context.
 * @param data     Input bytes.
 * @param len      Number of input bytes.
 * @param consumed [out] How many bytes were actually consumed.
 * @return HEATSHRINK_SINK_OK or HEATSHRINK_SINK_FULL.
 *         HEATSHRINK_SINK_ERR_PARAM on NULL args.
 * @thread_safe: no
 */
heatshrink_sink_res_t heatshrink_encoder_sink(heatshrink_encoder_t *enc,
                                              const uint8_t *data,
                                              uint16_t len,
                                              uint16_t *consumed);

/**
 * Poll for compressed output bytes.
 *
 * @param enc      Encoder context.
 * @param out      Output buffer.
 * @param out_size Size of output buffer.
 * @param produced [out] How many bytes were written to out.
 * @return HEATSHRINK_POLL_MORE or HEATSHRINK_POLL_EMPTY.
 *         HEATSHRINK_POLL_ERR_PARAM on NULL args.
 * @thread_safe: no
 */
heatshrink_poll_res_t heatshrink_encoder_poll(heatshrink_encoder_t *enc,
                                              uint8_t *out,
                                              uint16_t out_size,
                                              uint16_t *produced);

/**
 * Signal end of input.  Keep calling poll() until FINISH_DONE.
 * @thread_safe: no
 */
heatshrink_finish_res_t heatshrink_encoder_finish(heatshrink_encoder_t *enc);

/* ---- Decoder ---- */

typedef enum {
    HS_DEC_READY      = 0,
    HS_DEC_TAG        = 1,
    HS_DEC_LITERAL    = 2,
    HS_DEC_BACKREF_INDEX = 3,
    HS_DEC_BACKREF_COUNT = 4,
    HS_DEC_YIELD_LITERAL = 5,
    HS_DEC_YIELD_BACKREF = 6,
    HS_DEC_FINISHING  = 7,
    HS_DEC_DONE       = 8,
} heatshrink_dec_state_t;

typedef struct {
    uint8_t            *window;       /* caller-provided output window   */
    uint16_t            window_size;
    uint8_t             window_bits;
    uint8_t             lookahead_bits;

    heatshrink_dec_state_t state;

    /* input bit buffer */
    uint8_t            *input_buf;    /* points into window (shared)     */
    uint16_t            input_size;   /* bytes buffered                  */
    uint16_t            input_index;  /* read cursor                     */

    /* bit-level input accumulator */
    uint16_t            current_byte; /* current byte being consumed     */
    uint8_t             bit_idx;      /* bits remaining in current_byte  */

    /* output window tracking */
    uint16_t            head_index;   /* write cursor in window          */
    uint16_t            output_count; /* bytes pending output            */
    uint16_t            output_index; /* read cursor for output          */

    /* back-reference state */
    uint16_t            match_pos;
    uint16_t            match_len;

    uint8_t             finishing;
} heatshrink_decoder_t;

/**
 * Initialise the decoder.
 *
 * @param dec            Caller-allocated decoder.  Must not be NULL.
 * @param window_buf     Buffer of size (window_size + window_size/2).
 *                       The extra half is used as input staging buffer.
 *                       Must not be NULL.
 * @param window_size    Must match encoder's window_size (power-of-2, 256..2048).
 * @param lookahead_bits Must match encoder's lookahead_bits (4..8).
 * @return HEATSHRINK_OK on success, HEATSHRINK_ERR_PARAM on bad args.
 * @thread_safe: no
 */
heatshrink_err_t heatshrink_decoder_init(heatshrink_decoder_t *dec,
                                         uint8_t *window_buf,
                                         uint16_t window_size,
                                         uint8_t lookahead_bits);

/**
 * Reset decoder to initial state (reuse same buffers).
 * @thread_safe: no
 */
heatshrink_err_t heatshrink_decoder_reset(heatshrink_decoder_t *dec);

/**
 * Feed compressed data to the decoder.
 *
 * @param dec      Decoder context.
 * @param data     Compressed input bytes.
 * @param len      Number of bytes.
 * @param consumed [out] How many bytes were actually consumed.
 * @return HEATSHRINK_SINK_OK or HEATSHRINK_SINK_FULL.
 *         HEATSHRINK_SINK_ERR_PARAM on NULL args.
 * @thread_safe: no
 */
heatshrink_sink_res_t heatshrink_decoder_sink(heatshrink_decoder_t *dec,
                                              const uint8_t *data,
                                              uint16_t len,
                                              uint16_t *consumed);

/**
 * Poll for decompressed output bytes.
 *
 * @param dec      Decoder context.
 * @param out      Output buffer.
 * @param out_size Size of output buffer.
 * @param produced [out] How many bytes were written to out.
 * @return HEATSHRINK_POLL_MORE or HEATSHRINK_POLL_EMPTY.
 *         HEATSHRINK_POLL_ERR_PARAM on NULL args.
 * @thread_safe: no
 */
heatshrink_poll_res_t heatshrink_decoder_poll(heatshrink_decoder_t *dec,
                                              uint8_t *out,
                                              uint16_t out_size,
                                              uint16_t *produced);

/**
 * Signal end of compressed input.  Keep calling poll() until FINISH_DONE.
 * @thread_safe: no
 */
heatshrink_finish_res_t heatshrink_decoder_finish(heatshrink_decoder_t *dec);

#ifdef __cplusplus
}
#endif

#endif /* HEATSHRINK_H */
