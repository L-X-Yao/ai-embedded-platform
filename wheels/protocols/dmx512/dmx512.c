// SPDX-License-Identifier: MIT
/*
 * dmx512 -- DMX512 lighting protocol data layer
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "dmx512.h"
#include "contract.h"
#include <string.h>

/* ---------- init ---------- */

dmx512_err_t dmx512_init(dmx512_t *ctx, uint8_t *buf,
                          uint16_t num_channels)
{
    if (!ctx || !buf) return DMX512_ERR_PARAM;
    if (num_channels == 0 || num_channels > DMX512_MAX_CHANNELS)
        return DMX512_ERR_RANGE;

    ctx->buf          = buf;
    ctx->num_channels = num_channels;

    /* Clear: start code + all channels */
    memset(buf, 0, (size_t)(num_channels + 1));
    buf[0] = DMX512_START_CODE;

    return DMX512_OK;
}

/* ---------- set/get ---------- */

dmx512_err_t dmx512_set_channel(dmx512_t *ctx,
                                 uint16_t channel, uint8_t value)
{
    if (!ctx || !ctx->buf) return DMX512_ERR_PARAM;
    if (channel == 0 || channel > ctx->num_channels)
        return DMX512_ERR_RANGE;

    ctx->buf[channel] = value;  /* 1-based: buf[1] = ch1 */
    return DMX512_OK;
}

dmx512_err_t dmx512_get_channel(const dmx512_t *ctx,
                                 uint16_t channel, uint8_t *value)
{
    if (!ctx || !ctx->buf || !value) return DMX512_ERR_PARAM;
    if (channel == 0 || channel > ctx->num_channels)
        return DMX512_ERR_RANGE;

    *value = ctx->buf[channel];
    return DMX512_OK;
}

dmx512_err_t dmx512_set_range(dmx512_t *ctx,
                               uint16_t start_channel,
                               const uint8_t *values, uint16_t count)
{
    if (!ctx || !ctx->buf || !values) return DMX512_ERR_PARAM;
    if (count == 0) return DMX512_ERR_PARAM;
    if (start_channel == 0 || start_channel > ctx->num_channels)
        return DMX512_ERR_RANGE;
    if ((uint32_t)start_channel + count - 1 > ctx->num_channels)
        return DMX512_ERR_RANGE;

    memcpy(&ctx->buf[start_channel], values, count);
    ENSURE(start_channel + count - 1 <= ctx->num_channels);
    return DMX512_OK;
}

/* ---------- packet ---------- */

dmx512_err_t dmx512_get_packet(const dmx512_t *ctx,
                                const uint8_t **buf, size_t *len)
{
    if (!ctx || !ctx->buf || !buf || !len) return DMX512_ERR_PARAM;

    *buf = ctx->buf;
    *len = (size_t)(ctx->num_channels + 1);
    return DMX512_OK;
}

dmx512_err_t dmx512_parse_packet(dmx512_t *ctx,
                                  const uint8_t *data, size_t len)
{
    if (!ctx || !ctx->buf || !data) return DMX512_ERR_PARAM;
    if (len < 1) return DMX512_ERR_SHORT;

    /* Verify start code */
    if (data[0] != DMX512_START_CODE) return DMX512_ERR_FORMAT;

    /* Copy channel data (skip start code). Only copy up to our channel count. */
    size_t data_channels = len - 1;
    size_t copy_count = (data_channels < ctx->num_channels)
                        ? data_channels : ctx->num_channels;

    ctx->buf[0] = DMX512_START_CODE;
    if (copy_count > 0) {
        memcpy(&ctx->buf[1], &data[1], copy_count);
    }

    /* Zero out any remaining channels if received fewer than configured */
    if (copy_count < ctx->num_channels) {
        memset(&ctx->buf[1 + copy_count], 0,
               (size_t)(ctx->num_channels - copy_count));
    }

    return DMX512_OK;
}
