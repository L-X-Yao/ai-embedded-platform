// SPDX-License-Identifier: MIT
/*
 * retry — configurable retry strategy with backoff
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "retry.h"
#include "contract.h"

void retry_init(retry_ctx_t *ctx, const retry_config_t *config)
{
    if (!ctx || !config)
        return;
    ctx->config = *config;
    ctx->attempt = 0;
    ctx->next_delay_ms = 0;
    ENSURE(ctx->attempt == 0);
}

static uint32_t calc_delay(const retry_ctx_t *ctx)
{
    uint32_t delay;
    uint8_t n = ctx->attempt;

    switch (ctx->config.strategy) {
    case RETRY_LINEAR: {
        uint32_t mult = (uint32_t)(n + 1);
        if (ctx->config.base_delay_ms != 0 &&
            mult > ctx->config.max_delay_ms / ctx->config.base_delay_ms) {
            delay = ctx->config.max_delay_ms;
        } else {
            delay = ctx->config.base_delay_ms * mult;
        }
        break;
    }
    case RETRY_EXPONENTIAL: {
        delay = ctx->config.base_delay_ms;
        for (uint8_t i = 0; i < n; i++) {
            if (delay >= ctx->config.max_delay_ms / 2) {
                delay = ctx->config.max_delay_ms;
                break;
            }
            delay *= 2;
        }
        break;
    }
    default:
        delay = ctx->config.base_delay_ms;
        break;
    }

    if (delay > ctx->config.max_delay_ms)
        delay = ctx->config.max_delay_ms;
    return delay;
}

bool retry_should_retry(retry_ctx_t *ctx)
{
    if (!ctx)
        return false;
    if (ctx->attempt >= ctx->config.max_attempts)
        return false;
    ctx->next_delay_ms = calc_delay(ctx);
    ctx->attempt++;
    ENSURE(ctx->attempt <= ctx->config.max_attempts);
    ENSURE(ctx->next_delay_ms <= ctx->config.max_delay_ms);
    return true;
}

uint32_t retry_get_delay(const retry_ctx_t *ctx)
{
    if (!ctx)
        return 0;
    return ctx->next_delay_ms;
}

uint8_t retry_get_attempt(const retry_ctx_t *ctx)
{
    if (!ctx)
        return 0;
    return ctx->attempt;
}

void retry_reset(retry_ctx_t *ctx)
{
    if (!ctx)
        return;
    ctx->attempt = 0;
    ctx->next_delay_ms = 0;
}
