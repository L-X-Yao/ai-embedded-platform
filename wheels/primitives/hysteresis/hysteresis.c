// SPDX-License-Identifier: MIT
/*
 * hysteresis — threshold comparator with hysteresis band
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "hysteresis.h"
#include "contract.h"

/* NaN guard: IEEE 754 — NaN != NaN */
static bool hyst_is_nan(float x)
{
    return !(x == x);
}

static bool hyst_thresholds_valid(float low, float high)
{
    if (hyst_is_nan(low))  return false;
    if (hyst_is_nan(high)) return false;
    if (!(low < high))     return false;   /* catches low >= high and NaN */
    return true;
}

hysteresis_err_t hysteresis_init(hysteresis_t *ctx,
                                 float thresh_low, float thresh_high)
{
    if (!ctx) return HYSTERESIS_ERR_PARAM;
    if (!hyst_thresholds_valid(thresh_low, thresh_high))
        return HYSTERESIS_ERR_PARAM;

    ctx->thresh_low  = thresh_low;
    ctx->thresh_high = thresh_high;
    ctx->state       = false;
    ctx->primed      = false;

    ENSURE(!ctx->primed);
    return HYSTERESIS_OK;
}

bool hysteresis_feed(hysteresis_t *ctx, float value)
{
    if (!ctx) return false;

    /* NaN input: keep current state unchanged */
    if (hyst_is_nan(value)) return ctx->state;

    if (!ctx->primed)
    {
        /* First sample: set state based on which threshold is closer */
        float mid = ctx->thresh_low
                    + (ctx->thresh_high - ctx->thresh_low) * 0.5f;
        ctx->state  = (value >= mid);
        ctx->primed = true;
    }

    /* Standard hysteresis logic */
    if (!ctx->state && value >= ctx->thresh_high)
    {
        ctx->state = true;
    }
    else if (ctx->state && value <= ctx->thresh_low)
    {
        ctx->state = false;
    }

    INVARIANT(ctx->primed);
    return ctx->state;
}

bool hysteresis_get_state(const hysteresis_t *ctx)
{
    if (!ctx) return false;
    return ctx->state;
}

hysteresis_err_t hysteresis_reset(hysteresis_t *ctx)
{
    if (!ctx) return HYSTERESIS_ERR_PARAM;

    ctx->state  = false;
    ctx->primed = false;

    ENSURE(!ctx->primed);
    return HYSTERESIS_OK;
}

hysteresis_err_t hysteresis_set_thresholds(hysteresis_t *ctx,
                                           float thresh_low,
                                           float thresh_high)
{
    if (!ctx) return HYSTERESIS_ERR_PARAM;
    if (!hyst_thresholds_valid(thresh_low, thresh_high))
        return HYSTERESIS_ERR_PARAM;

    ctx->thresh_low  = thresh_low;
    ctx->thresh_high = thresh_high;

    ENSURE(ctx->thresh_low < ctx->thresh_high);
    return HYSTERESIS_OK;
}
