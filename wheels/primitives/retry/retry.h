// SPDX-License-Identifier: MIT
/*
 * retry — configurable retry strategy with backoff
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
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
} retry_t;

/** @thread_safe: no @isr_safe: no */
void retry_init(retry_t *ctx, const retry_config_t *config);

/** Return true if attempts remain and advance to next attempt. @thread_safe: no @isr_safe: no */
bool retry_should_retry(retry_t *ctx);

/** Return the computed delay in ms for the current attempt. @thread_safe: no @isr_safe: no */
uint32_t retry_get_delay(const retry_t *ctx);

/** Return the current attempt number (0-based). @thread_safe: no @isr_safe: yes */
uint8_t retry_get_attempt(const retry_t *ctx);

/** Reset attempt counter and delay to initial state. @thread_safe: no @isr_safe: no */
void retry_reset(retry_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* RETRY_H */
