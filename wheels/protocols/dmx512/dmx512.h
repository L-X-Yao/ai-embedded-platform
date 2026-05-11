// SPDX-License-Identifier: MIT
/*
 * dmx512 -- DMX512 lighting protocol data layer
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef DMX512_H
#define DMX512_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define DMX512_START_CODE      0x00
#define DMX512_MAX_CHANNELS    512
#define DMX512_PACKET_SIZE     513  /* start code + 512 channels */

/* ---------- error codes ---------- */

typedef enum {
    DMX512_OK           =  0,
    DMX512_ERR_PARAM    = -1,
    DMX512_ERR_RANGE    = -2,
    DMX512_ERR_SHORT    = -3,
    DMX512_ERR_OVERFLOW = -4,
    DMX512_ERR_FORMAT   = -5,
} dmx512_err_t;

/* ---------- context ---------- */

/** DMX512 universe context. Caller provides a 513-byte buffer. */
typedef struct {
    uint8_t  *buf;           /* 513-byte buffer: [start_code][ch1]..[ch512] */
    uint16_t  num_channels;  /* active channel count (1..512) */
} dmx512_ctx_t;

/* ---------- API ---------- */

/** Initialize DMX512 context with caller-provided buffer.
 *  @param buf  Must be at least (num_channels + 1) bytes.
 *  @param num_channels  Number of active channels (1..512).
 *  @thread_safe: no  @isr_safe: no */
dmx512_err_t dmx512_init(dmx512_ctx_t *ctx, uint8_t *buf,
                          uint16_t num_channels);

/** Set a single channel value (1-based channel number, 0-255 value).
 *  @thread_safe: caller_must_lock  @isr_safe: no */
dmx512_err_t dmx512_set_channel(dmx512_ctx_t *ctx,
                                 uint16_t channel, uint8_t value);

/** Get a single channel value (1-based channel number).
 *  @thread_safe: caller_must_lock  @isr_safe: no */
dmx512_err_t dmx512_get_channel(const dmx512_ctx_t *ctx,
                                 uint16_t channel, uint8_t *value);

/** Set a range of channels starting at start_channel (1-based).
 *  @thread_safe: caller_must_lock  @isr_safe: no */
dmx512_err_t dmx512_set_range(dmx512_ctx_t *ctx,
                               uint16_t start_channel,
                               const uint8_t *values, uint16_t count);

/** Get raw packet bytes for transmission (start code + channel data).
 *  @param buf   Receives pointer to internal buffer (do not free).
 *  @param len   Receives packet length (1 + num_channels).
 *  @thread_safe: caller_must_lock  @isr_safe: no */
dmx512_err_t dmx512_get_packet(const dmx512_ctx_t *ctx,
                                const uint8_t **buf, size_t *len);

/** Parse received raw bytes into the universe buffer.
 *  @param data  Raw DMX packet (start code + channel data).
 *  @param len   Length of data (at least 1 for start code).
 *  @thread_safe: caller_must_lock  @isr_safe: no */
dmx512_err_t dmx512_parse_packet(dmx512_ctx_t *ctx,
                                  const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DMX512_H */
