// SPDX-License-Identifier: MIT
/*
 * timeout — deadline-based timeout tracking
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Uses caller-provided tick value (no HAL dependency).
 * Handles 32-bit tick wraparound correctly.
 * Thread safety: caller owns the timeout_t struct.
 */

#ifndef TIMEOUT_H
#define TIMEOUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t start;
    uint32_t duration;
} timeout_t;

/** @thread_safe: no */
void timeout_set(timeout_t *t, uint32_t now, uint32_t duration);

/** Return true if the deadline has passed (handles tick wraparound). @thread_safe: no */
bool timeout_expired(const timeout_t *t, uint32_t now);

/** Return ticks remaining until deadline (0 if expired). @thread_safe: no */
uint32_t timeout_remaining(const timeout_t *t, uint32_t now);

/** Return ticks elapsed since the timeout was set. @thread_safe: no */
uint32_t timeout_elapsed(const timeout_t *t, uint32_t now);

/** Restart the timeout from now with the same duration. @thread_safe: no */
void timeout_restart(timeout_t *t, uint32_t now);

/** Add extra ticks to the current deadline. @thread_safe: no */
void timeout_extend(timeout_t *t, uint32_t extra);

#ifdef __cplusplus
}
#endif

#endif /* TIMEOUT_H */
