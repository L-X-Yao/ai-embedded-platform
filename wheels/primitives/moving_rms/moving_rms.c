// SPDX-License-Identifier: MIT
/*
 * moving_rms — running root-mean-square over a sliding window
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "moving_rms.h"
#include <float.h>
#include <math.h>

moving_rms_err_t moving_rms_init(moving_rms_t *ctx, float *buf, size_t window)
{
    if (!ctx || !buf || window == 0)
        return MOVING_RMS_ERR_PARAM;
    ctx->buf = buf;
    ctx->cap = window;
    ctx->head = 0;
    ctx->count = 0;
    ctx->sum_sq = 0.0f;
    for (size_t i = 0; i < window; i++)
        buf[i] = 0.0f;
    return MOVING_RMS_OK;
}

float moving_rms_feed(moving_rms_t *ctx, float sample)
{
    if (!ctx)
        return 0.0f;

    float sq = sample * sample;
    float sum_sq = ctx->sum_sq - ctx->buf[ctx->head] + sq;

    /* reject NaN, Inf and a sample that overflows the sum: inf - inf would
     * leave the running sum NaN for good */
    if (!(sum_sq >= -FLT_MAX && sum_sq <= FLT_MAX))
        return moving_rms_get(ctx);

    ctx->buf[ctx->head] = sq;
    ctx->sum_sq = sum_sq;
    if (ctx->sum_sq < 0.0f)
        ctx->sum_sq = 0.0f;

    ctx->head = (ctx->head + 1) % ctx->cap;
    if (ctx->count < ctx->cap)
        ctx->count++;

    return sqrtf(ctx->sum_sq / (float)ctx->count);
}

float moving_rms_get(const moving_rms_t *ctx)
{
    if (!ctx || ctx->count == 0)
        return 0.0f;
    float mean_sq = ctx->sum_sq / (float)ctx->count;
    if (mean_sq < 0.0f)
        mean_sq = 0.0f;
    return sqrtf(mean_sq);
}

moving_rms_err_t moving_rms_reset(moving_rms_t *ctx)
{
    if (!ctx)
        return MOVING_RMS_ERR_PARAM;
    ctx->head = 0;
    ctx->count = 0;
    ctx->sum_sq = 0.0f;
    for (size_t i = 0; i < ctx->cap; i++)
        ctx->buf[i] = 0.0f;
    return MOVING_RMS_OK;
}
