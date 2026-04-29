/*
 * retry — configurable retry strategy with backoff
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Pure computation: calculates next delay, caller provides the actual
 * wait mechanism (RTOS delay / busy-wait / timer).
 * Thread safety: caller owns the retry_ctx struct.
 */

#ifndef RETRY_H
#define RETRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RETRY_FIXED,
    RETRY_LINEAR,
    RETRY_EXPONENTIAL,
} retry_strategy_t;

typedef struct {
    retry_strategy_t strategy;
    uint32_t base_delay_ms;
    uint32_t max_delay_ms;
    uint8_t  max_attempts;
} retry_config_t;

typedef struct {
    retry_config_t config;
    uint8_t  attempt;
    uint32_t next_delay_ms;
} retry_ctx_t;

/** @thread_safe: no */
void retry_init(retry_ctx_t *ctx, const retry_config_t *config);

bool retry_should_retry(retry_ctx_t *ctx);

uint32_t retry_get_delay(const retry_ctx_t *ctx);

uint8_t retry_get_attempt(const retry_ctx_t *ctx);

void retry_reset(retry_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* RETRY_H */
