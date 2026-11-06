// SPDX-License-Identifier: MIT
/*
 * moving_average — sliding window and exponential moving average filters
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "moving_average.h"
#include "contract.h"
#include <float.h>
#include <string.h>

/* False for NaN as well: every comparison with NaN is false. */
static bool is_finite(float x)
{
    return x >= -FLT_MAX && x <= FLT_MAX;
}

/* --- SMA --- */

moving_average_err_t moving_average_sma_init(moving_average_sma_t *ctx,
                                             float *buf, size_t window)
{
    if (!ctx || !buf || window == 0)
    {
        return MOVING_AVERAGE_ERR_PARAM;
    }
    ctx->buf = buf;
    ctx->window = window;
    ctx->count = 0;
    ctx->head = 0;
    ctx->sum = 0.0f;
    memset(buf, 0, window * sizeof(float));
    ENSURE(ctx->count == 0 && ctx->head == 0);
    return MOVING_AVERAGE_OK;
}

moving_average_err_t moving_average_sma_reset(moving_average_sma_t *ctx)
{
    if (!ctx)
    {
        return MOVING_AVERAGE_ERR_PARAM;
    }
    ctx->count = 0;
    ctx->head = 0;
    ctx->sum = 0.0f;
    if (ctx->buf)
    {
        memset(ctx->buf, 0, ctx->window * sizeof(float));
    }
    return MOVING_AVERAGE_OK;
}

float moving_average_sma_feed(moving_average_sma_t *ctx, float sample)
{
    if (!ctx || !ctx->buf)
    {
        return 0.0f;
    }
    /* reject NaN, Inf and a sample that overflows the sum — the running sum
     * would never recover (inf - x is still inf) and corrupt all averages */
    float evicted = ctx->count >= ctx->window ? ctx->buf[ctx->head] : 0.0f;
    float sum = ctx->sum - evicted + sample;
    if (!is_finite(sum))
    {
        return ctx->count > 0 ? ctx->sum / (float)ctx->count : 0.0f;
    }

    ctx->buf[ctx->head] = sample;
    ctx->sum = sum;
    ctx->head = (ctx->head + 1) % ctx->window;

    if (ctx->count < ctx->window)
    {
        ctx->count++;
    }

    INVARIANT(ctx->count <= ctx->window);
    INVARIANT(ctx->head < ctx->window);
    return ctx->sum / (float)ctx->count;
}

float moving_average_sma_get(const moving_average_sma_t *ctx)
{
    if (!ctx || ctx->count == 0)
    {
        return 0.0f;
    }
    return ctx->sum / (float)ctx->count;
}

bool moving_average_sma_is_full(const moving_average_sma_t *ctx)
{
    if (!ctx)
    {
        return false;
    }
    return ctx->count >= ctx->window;
}

/* --- EMA --- */

moving_average_err_t moving_average_ema_init(moving_average_ema_t *ctx, float alpha)
{
    if (!ctx || !(alpha > 0.0f && alpha <= 1.0f))
    {
        return MOVING_AVERAGE_ERR_PARAM;
    }
    ctx->alpha = alpha;
    ctx->value = 0.0f;
    ctx->primed = false;
    return MOVING_AVERAGE_OK;
}

moving_average_err_t moving_average_ema_reset(moving_average_ema_t *ctx)
{
    if (!ctx)
    {
        return MOVING_AVERAGE_ERR_PARAM;
    }
    ctx->value = 0.0f;
    ctx->primed = false;
    return MOVING_AVERAGE_OK;
}

float moving_average_ema_feed(moving_average_ema_t *ctx, float sample)
{
    if (!ctx)
    {
        return 0.0f;
    }
    /* reject NaN and Inf — would poison EMA state permanently */
    if (!is_finite(sample))
    {
        return ctx->value;
    }
    if (!ctx->primed)
    {
        ctx->value = sample;
        ctx->primed = true;
    }
    else
    {
        ctx->value = ctx->alpha * sample + (1.0f - ctx->alpha) * ctx->value;
    }
    return ctx->value;
}

float moving_average_ema_get(const moving_average_ema_t *ctx)
{
    if (!ctx)
    {
        return 0.0f;
    }
    return ctx->value;
}
