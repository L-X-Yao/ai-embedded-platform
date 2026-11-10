// SPDX-License-Identifier: MIT
/*
 * ewma — integer-only exponential weighted moving average (bit-shift alpha)
 *
 * Layer:     L0
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "ewma.h"
#include "contract.h"

ewma_err_t ewma_init(ewma_t *ctx, uint8_t shift)
{
    if (!ctx)              return EWMA_ERR_PARAM;
    if (shift == 0)        return EWMA_ERR_PARAM;
    if (shift > 16)        return EWMA_ERR_PARAM;

    ctx->acc    = 0;
    ctx->value  = 0;
    ctx->shift  = shift;
    ctx->primed = false;

    ENSURE(!ctx->primed);
    return EWMA_OK;
}

int32_t ewma_feed(ewma_t *ctx, int32_t sample)
{
    if (!ctx) return 0;

    if (!ctx->primed)
    {
        /* A multiply, because shifting a negative value left is undefined. */
        ctx->acc    = (int64_t)sample * ((int64_t)1 << ctx->shift);
        ctx->primed = true;
    }
    else
    {
        /*
         * acc holds y * 2^shift, so y += (x - y) / 2^shift becomes
         *
         *     acc += x - (acc >> shift)
         *
         * The sample goes in whole: shifting it or y on its own would throw
         * away the low bits on every feed and stall short of the input.
         * The right shift must round down, as it does on every compiler
         * this library targets (C99 leaves the choice to the implementation).
         * With shift <= 16, |acc| stays below 2^48.
         */
        ctx->acc += (int64_t)sample - (ctx->acc >> ctx->shift);
    }

    /* Always fits: the state stays between the extremes of the input. */
    int32_t out = (int32_t)(ctx->acc >> ctx->shift);
    ctx->value = out;

    INVARIANT(ctx->primed);
    return out;
}

int32_t ewma_get(const ewma_t *ctx)
{
    if (!ctx) return 0;
    return ctx->value;
}

ewma_err_t ewma_reset(ewma_t *ctx)
{
    if (!ctx) return EWMA_ERR_PARAM;

    ctx->acc    = 0;
    ctx->value  = 0;
    ctx->primed = false;

    ENSURE(!ctx->primed);
    return EWMA_OK;
}
