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
        ctx->value  = sample;
        ctx->primed = true;
    }
    else
    {
        /*
         * Integer EWMA: value = value - (value >> shift) + (sample >> shift)
         *
         * Equivalent to: value = value * (1 - 1/2^shift) + sample * (1/2^shift)
         * All integer arithmetic, no float needed.
         */
        ctx->value = ctx->value - (ctx->value >> ctx->shift)
                                + (sample    >> ctx->shift);
    }

    INVARIANT(ctx->primed);
    return ctx->value;
}

int32_t ewma_get(const ewma_t *ctx)
{
    if (!ctx) return 0;
    return ctx->value;
}

ewma_err_t ewma_reset(ewma_t *ctx)
{
    if (!ctx) return EWMA_ERR_PARAM;

    ctx->value  = 0;
    ctx->primed = false;

    ENSURE(!ctx->primed);
    return EWMA_OK;
}
