// SPDX-License-Identifier: MIT
/*
 * signal_stats — running statistics: mean, variance, min, max, count
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "signal_stats.h"
#include <float.h>

signal_stats_err_t signal_stats_init(signal_stats_t *ctx)
{
    if (!ctx)
        return SIGNAL_STATS_ERR_PARAM;
    ctx->mean = 0.0f;
    ctx->m2 = 0.0f;
    ctx->min_val = FLT_MAX;
    ctx->max_val = -FLT_MAX;
    ctx->count = 0;
    return SIGNAL_STATS_OK;
}

signal_stats_err_t signal_stats_feed(signal_stats_t *ctx, float sample)
{
    if (!ctx)
        return SIGNAL_STATS_ERR_PARAM;
    if (!(sample == sample))
        return SIGNAL_STATS_ERR_PARAM;

    ctx->count++;
    /* Welford's online algorithm */
    float delta = sample - ctx->mean;
    ctx->mean += delta / (float)ctx->count;
    float delta2 = sample - ctx->mean;
    ctx->m2 += delta * delta2;

    if (sample < ctx->min_val) ctx->min_val = sample;
    if (sample > ctx->max_val) ctx->max_val = sample;

    return SIGNAL_STATS_OK;
}

float signal_stats_mean(const signal_stats_t *ctx)
{
    if (!ctx || ctx->count == 0)
        return 0.0f;
    return ctx->mean;
}

float signal_stats_variance(const signal_stats_t *ctx)
{
    if (!ctx || ctx->count < 1)
        return 0.0f;
    return ctx->m2 / (float)ctx->count;
}

float signal_stats_min(const signal_stats_t *ctx)
{
    if (!ctx || ctx->count == 0)
        return 0.0f;
    return ctx->min_val;
}

float signal_stats_max(const signal_stats_t *ctx)
{
    if (!ctx || ctx->count == 0)
        return 0.0f;
    return ctx->max_val;
}

uint32_t signal_stats_count(const signal_stats_t *ctx)
{
    if (!ctx)
        return 0;
    return ctx->count;
}

float signal_stats_range(const signal_stats_t *ctx)
{
    if (!ctx || ctx->count == 0)
        return 0.0f;
    return ctx->max_val - ctx->min_val;
}

signal_stats_err_t signal_stats_reset(signal_stats_t *ctx)
{
    if (!ctx)
        return SIGNAL_STATS_ERR_PARAM;
    return signal_stats_init(ctx);
}
