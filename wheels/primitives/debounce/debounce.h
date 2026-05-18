// SPDX-License-Identifier: MIT
/*
 * debounce — software button debouncing
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Feed raw pin state at fixed intervals. Output stable state after
 * N consecutive identical readings. Pure C99, no heap, no timer HW.
 * Thread safety: caller owns the debounce_t struct.
 */

#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  threshold;
    uint8_t  counter;
    bool     raw;
    bool     stable;
    bool     changed;
} debounce_t;

/** @thread_safe: no @isr_safe: no */
void debounce_init(debounce_t *d, uint8_t threshold, bool initial_state);

/** Feed a raw sample and return current stable state. @thread_safe: no @isr_safe: yes */
bool debounce_update(debounce_t *d, bool raw);

/** Return current stable (debounced) state. @thread_safe: no @isr_safe: yes */
bool debounce_get(const debounce_t *d);

/** Return true if stable state changed on last update. @thread_safe: no @isr_safe: yes */
bool debounce_changed(const debounce_t *d);

/** Return true if a rising edge (false->true) occurred on last update. @thread_safe: no @isr_safe: yes */
bool debounce_rising(const debounce_t *d);

/** Return true if a falling edge (true->false) occurred on last update. @thread_safe: no @isr_safe: yes */
bool debounce_falling(const debounce_t *d);

#ifdef __cplusplus
}
#endif

#endif /* DEBOUNCE_H */
