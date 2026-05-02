/*
 * rate_limiter — generic token bucket rate limiter.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "rate_limiter.h"

/* Refill tokens based on elapsed ticks. Overflow-safe: uses unsigned
 * wraparound for tick delta, and clamps accumulated tokens to max. */
static void refill(rate_limiter_t *rl, uint32_t current_tick) {
    uint32_t elapsed = current_tick - rl->last_tick;
    if (elapsed == 0) {
        return;
    }

    /* Calculate tokens to add. Guard against overflow:
     * if elapsed * refill_rate would overflow uint32_t, clamp to max. */
    uint32_t add;
    if (rl->refill_rate > 0 && elapsed > UINT32_MAX / rl->refill_rate) {
        add = rl->max_tokens; /* overflow — just fill to max */
    } else {
        add = elapsed * rl->refill_rate;
    }

    uint32_t new_tokens = rl->tokens + add;
    /* Clamp to max (also handles addition overflow) */
    if (new_tokens < rl->tokens || new_tokens > rl->max_tokens) {
        new_tokens = rl->max_tokens;
    }

    rl->tokens = new_tokens;
    rl->last_tick = current_tick;
}

rate_limiter_err_t rate_limiter_init(rate_limiter_t *rl, uint32_t max_tokens,
                                     uint32_t tokens_per_tick) {
    if (!rl || max_tokens == 0 || tokens_per_tick == 0) {
        return RATE_LIMITER_ERR_PARAM;
    }

    /* Guard against overflow when scaling to fixed-point representation */
    if (max_tokens > UINT32_MAX / RATE_LIMITER_SCALE ||
        tokens_per_tick > UINT32_MAX / RATE_LIMITER_SCALE) {
        return RATE_LIMITER_ERR_PARAM;
    }

    rl->max_tokens   = max_tokens * RATE_LIMITER_SCALE;
    rl->refill_rate  = tokens_per_tick * RATE_LIMITER_SCALE;
    rl->tokens       = rl->max_tokens;
    rl->last_tick    = 0;
    rl->allowed_count = 0;
    rl->denied_count  = 0;

    return RATE_LIMITER_OK;
}

rate_limiter_err_t rate_limiter_try(rate_limiter_t *rl, uint32_t current_tick) {
    return rate_limiter_try_n(rl, current_tick, 1);
}

rate_limiter_err_t rate_limiter_try_n(rate_limiter_t *rl, uint32_t current_tick,
                                      uint32_t n) {
    if (!rl) {
        return RATE_LIMITER_ERR_PARAM;
    }
    if (n == 0) {
        return RATE_LIMITER_OK;
    }

    refill(rl, current_tick);

    uint32_t cost = n * RATE_LIMITER_SCALE;
    /* Guard against n * SCALE overflow */
    if (cost / RATE_LIMITER_SCALE != n) {
        /* Overflow — request exceeds any possible bucket */
        if (rl->denied_count < UINT32_MAX) {
            rl->denied_count++;
        }
        return RATE_LIMITER_DENIED;
    }

    if (rl->tokens >= cost) {
        rl->tokens -= cost;
        if (rl->allowed_count < UINT32_MAX) {
            rl->allowed_count++;
        }
        return RATE_LIMITER_OK;
    }

    if (rl->denied_count < UINT32_MAX) {
        rl->denied_count++;
    }
    return RATE_LIMITER_DENIED;
}

bool rate_limiter_can_proceed(const rate_limiter_t *rl, uint32_t current_tick) {
    if (!rl) {
        return false;
    }

    /* Work on a copy to avoid mutating the const input.
     * This is safe because refill is pure arithmetic. */
    rate_limiter_t copy = *rl;
    refill(&copy, current_tick);

    return copy.tokens >= RATE_LIMITER_SCALE;
}

void rate_limiter_reset(rate_limiter_t *rl) {
    if (!rl) {
        return;
    }
    rl->tokens        = rl->max_tokens;
    rl->last_tick     = 0;
    rl->allowed_count = 0;
    rl->denied_count  = 0;
}

uint32_t rate_limiter_allowed(const rate_limiter_t *rl) {
    return rl ? rl->allowed_count : 0;
}

uint32_t rate_limiter_denied(const rate_limiter_t *rl) {
    return rl ? rl->denied_count : 0;
}

uint32_t rate_limiter_available(const rate_limiter_t *rl) {
    return rl ? rl->tokens / RATE_LIMITER_SCALE : 0;
}
