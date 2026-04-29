/*
 * debounce — software button debouncing
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 *
 * Feed raw pin state at fixed intervals. Output stable state after
 * N consecutive identical readings. Pure C99, no heap, no timer HW.
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

void debounce_init(debounce_t *d, uint8_t threshold, bool initial_state);

bool debounce_update(debounce_t *d, bool raw);

bool debounce_get(const debounce_t *d);

bool debounce_changed(const debounce_t *d);

bool debounce_rising(const debounce_t *d);

bool debounce_falling(const debounce_t *d);

#ifdef __cplusplus
}
#endif

#endif /* DEBOUNCE_H */
