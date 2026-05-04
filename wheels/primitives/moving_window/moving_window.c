// SPDX-License-Identifier: MIT
/*
 * moving_window — sliding window statistics (min/max/mean/variance)
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Min/max are recomputed on demand by scanning the buffer (O(n) per query).
 * Mean and variance use running sums (O(1) push, O(1) query).
 */

#include "moving_window.h"
#include "contract.h"
#include <string.h>

moving_window_err_t moving_window_init(moving_window_t *ctx,
                                       float *buf, size_t capacity)
{
    if (!ctx || !buf || capacity == 0)
    {
        return MOVING_WINDOW_ERR_PARAM;
    }
    ctx->buf      = buf;
    ctx->capacity = capacity;
    ctx->count    = 0;
    ctx->head     = 0;
    ctx->sum      = 0.0f;
    ctx->sum_sq   = 0.0f;
    memset(buf, 0, capacity * sizeof(float));
    ENSURE(ctx->count == 0 && ctx->head == 0);
    return MOVING_WINDOW_OK;
}

moving_window_err_t moving_window_push(moving_window_t *ctx, float value)
{
    if (!ctx || !ctx->buf)
    {
        return MOVING_WINDOW_ERR_PARAM;
    }
    /* reject NaN — would poison sum/sum_sq and corrupt all derived stats */
    if (value != value)
    {
        return MOVING_WINDOW_ERR_PARAM;
    }

    if (ctx->count >= ctx->capacity)
    {
        /* evict oldest */
        float old = ctx->buf[ctx->head];
        ctx->sum    -= old;
        ctx->sum_sq -= old * old;
    }

    ctx->buf[ctx->head] = value;
    ctx->sum    += value;
    ctx->sum_sq += value * value;
    ctx->head = (ctx->head + 1) % ctx->capacity;

    if (ctx->count < ctx->capacity)
    {
        ctx->count++;
    }

    INVARIANT(ctx->count <= ctx->capacity);
    INVARIANT(ctx->head < ctx->capacity);
    return MOVING_WINDOW_OK;
}

float moving_window_get_mean(const moving_window_t *ctx)
{
    if (!ctx || ctx->count == 0)
    {
        return 0.0f;
    }
    return ctx->sum / (float)ctx->count;
}

float moving_window_get_min(const moving_window_t *ctx)
{
    if (!ctx || ctx->count == 0)
    {
        return 0.0f;
    }
    /* scan buffer for current minimum */
    size_t start = 0;
    if (ctx->count == ctx->capacity)
    {
        start = 0;  /* full — all slots valid */
    }
    float min_val = ctx->buf[start];
    for (size_t i = 0; i < ctx->count; i++)
    {
        /* when not full, valid entries are [0..count-1].
         * when full, all capacity entries are valid. */
        size_t idx = i;
        if (ctx->buf[idx] < min_val)
        {
            min_val = ctx->buf[idx];
        }
    }
    return min_val;
}

float moving_window_get_max(const moving_window_t *ctx)
{
    if (!ctx || ctx->count == 0)
    {
        return 0.0f;
    }
    float max_val = ctx->buf[0];
    for (size_t i = 0; i < ctx->count; i++)
    {
        if (ctx->buf[i] > max_val)
        {
            max_val = ctx->buf[i];
        }
    }
    return max_val;
}

float moving_window_get_variance(const moving_window_t *ctx)
{
    if (!ctx || ctx->count == 0)
    {
        return 0.0f;
    }
    /* population variance = E[X^2] - (E[X])^2 */
    float n    = (float)ctx->count;
    float mean = ctx->sum / n;
    float var  = (ctx->sum_sq / n) - (mean * mean);
    /* clamp to zero in case of float rounding */
    return var < 0.0f ? 0.0f : var;
}

size_t moving_window_get_count(const moving_window_t *ctx)
{
    if (!ctx)
    {
        return 0;
    }
    return ctx->count;
}

bool moving_window_is_full(const moving_window_t *ctx)
{
    if (!ctx || ctx->capacity == 0)
    {
        return false;
    }
    return ctx->count >= ctx->capacity;
}

moving_window_err_t moving_window_reset(moving_window_t *ctx)
{
    if (!ctx)
    {
        return MOVING_WINDOW_ERR_PARAM;
    }
    ctx->count  = 0;
    ctx->head   = 0;
    ctx->sum    = 0.0f;
    ctx->sum_sq = 0.0f;
    if (ctx->buf)
    {
        memset(ctx->buf, 0, ctx->capacity * sizeof(float));
    }
    return MOVING_WINDOW_OK;
}
