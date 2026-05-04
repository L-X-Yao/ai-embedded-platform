/*
 * fault_inject.h — HAL fault injection for driver tests
 *
 * Header-only. Provides a fault context that can be wired into any HAL
 * callback to simulate bus errors, timeouts, and random failures.
 *
 * Usage:
 *   #include "fault_inject.h"
 *
 *   fault_ctx_t fi;
 *   fault_init(&fi);
 *   fault_fail_after(&fi, 3);  // fail starting from 4th call
 *
 *   // In HAL callback:
 *   if (FAULT_CHECK(&fi)) return false;  // injected failure
 */

#ifndef FAULT_INJECT_H
#define FAULT_INJECT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef enum {
    FAULT_MODE_NONE,
    FAULT_MODE_AFTER,
    FAULT_MODE_AT,
    FAULT_MODE_NEXT,
    FAULT_MODE_RANDOM
} fault_mode_t;

typedef struct {
    fault_mode_t mode;
    int          call_count;
    int          trigger;       /* threshold for AFTER / AT */
    uint8_t      random_pct;   /* 0-100 for RANDOM mode */
    uint32_t     prng_state;   /* xorshift32 state for RANDOM */
} fault_ctx_t;

__attribute__((unused))
static void fault_init(fault_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->mode = FAULT_MODE_NONE;
    ctx->trigger = -1;
    ctx->prng_state = 0x12345678u;
}

__attribute__((unused))
static void fault_fail_after(fault_ctx_t *ctx, int n)
{
    ctx->mode    = FAULT_MODE_AFTER;
    ctx->trigger = n;
}

__attribute__((unused))
static void fault_fail_at(fault_ctx_t *ctx, int n)
{
    ctx->mode    = FAULT_MODE_AT;
    ctx->trigger = n;
}

__attribute__((unused))
static void fault_fail_next(fault_ctx_t *ctx)
{
    ctx->mode = FAULT_MODE_NEXT;
}

__attribute__((unused))
static void fault_fail_random(fault_ctx_t *ctx, uint8_t pct)
{
    ctx->mode       = FAULT_MODE_RANDOM;
    ctx->random_pct = pct;
}

__attribute__((unused))
static void fault_reset(fault_ctx_t *ctx)
{
    fault_init(ctx);
}

__attribute__((unused))
static int fault_call_count(const fault_ctx_t *ctx)
{
    return ctx->call_count;
}

static uint32_t fault__xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/*
 * FAULT_CHECK — call on every HAL entry point.
 * Returns true if the call should be treated as a failure.
 */
#define FAULT_CHECK(ctx) fault__check(ctx)

static bool fault__check(fault_ctx_t *ctx)
{
    int n = ctx->call_count++;

    switch (ctx->mode) {
    case FAULT_MODE_AFTER:
        return n >= ctx->trigger;
    case FAULT_MODE_AT:
        return n == ctx->trigger;
    case FAULT_MODE_NEXT:
        ctx->mode = FAULT_MODE_NONE;
        return true;
    case FAULT_MODE_RANDOM: {
        uint32_t r = fault__xorshift32(&ctx->prng_state) % 100u;
        return r < ctx->random_pct;
    }
    default:
        return false;
    }
}

#endif /* FAULT_INJECT_H */
