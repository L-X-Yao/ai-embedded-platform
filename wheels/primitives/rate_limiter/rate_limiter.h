// SPDX-License-Identifier: MIT
/*
 * rate_limiter — generic token bucket rate limiter.
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-point scale factor: tokens are stored multiplied by 256
 * for sub-tick precision without floating point. */
#define RATE_LIMITER_SCALE 256U

typedef enum {
    RATE_LIMITER_OK         =  0,
    RATE_LIMITER_ERR_PARAM  = -1,
    RATE_LIMITER_DENIED     = -2,
} rate_limiter_err_t;

typedef struct {
    uint32_t tokens;          /* current token count (fixed-point, x256) */
    uint32_t max_tokens;      /* bucket capacity (fixed-point, x256) */
    uint32_t refill_rate;     /* tokens added per tick (fixed-point, x256) */
    uint32_t last_tick;       /* last refill timestamp */
    uint32_t allowed_count;   /* lifetime allowed calls */
    uint32_t denied_count;    /* lifetime denied calls */
} rate_limiter_t;

/* Initialize rate limiter.
 *   max_tokens:      bucket capacity (unscaled — e.g. 10 means 10 tokens)
 *   tokens_per_tick: refill rate per tick (unscaled — e.g. 1 means 1 token/tick)
 * Both are zero-guarded: returns ERR_PARAM if rl is NULL or either is 0.
 * @thread_safe: no — call during init @isr_safe: no */
rate_limiter_err_t rate_limiter_init(rate_limiter_t *rl, uint32_t max_tokens,
                                     uint32_t tokens_per_tick);

/* Try to consume one token.
 * Returns OK if allowed, DENIED if bucket empty.
 * @thread_safe: no @isr_safe: yes */
rate_limiter_err_t rate_limiter_try(rate_limiter_t *rl, uint32_t current_tick);

/* Try to consume N tokens at once.
 * Returns OK if N tokens available, DENIED otherwise (no partial consume).
 * @thread_safe: no @isr_safe: yes */
rate_limiter_err_t rate_limiter_try_n(rate_limiter_t *rl, uint32_t current_tick,
                                      uint32_t n);

/* Check if at least one token is available, without consuming.
 * @thread_safe: no @isr_safe: yes */
bool rate_limiter_can_proceed(const rate_limiter_t *rl, uint32_t current_tick);

/* Reset bucket to full and clear stats.
 * @thread_safe: no @isr_safe: no */
void rate_limiter_reset(rate_limiter_t *rl);

/* Return lifetime allowed count.
 * @thread_safe: no @isr_safe: yes */
uint32_t rate_limiter_allowed(const rate_limiter_t *rl);

/* Return lifetime denied count.
 * @thread_safe: no @isr_safe: yes */
uint32_t rate_limiter_denied(const rate_limiter_t *rl);

/* Return current available tokens (unscaled, truncated).
 * @thread_safe: no @isr_safe: yes */
uint32_t rate_limiter_available(const rate_limiter_t *rl);

#ifdef __cplusplus
}
#endif

#endif /* RATE_LIMITER_H */
