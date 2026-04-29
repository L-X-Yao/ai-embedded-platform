/*
 * debounce — software button debouncing
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */
#include "debounce.h"

void debounce_init(debounce_t *d, uint8_t threshold, bool initial_state)
{
    if (!d) return;
    d->threshold = threshold > 0 ? threshold : 1;
    d->counter = 0;
    d->raw = initial_state;
    d->stable = initial_state;
    d->changed = false;
}

bool debounce_update(debounce_t *d, bool raw)
{
    if (!d) return false;
    d->changed = false;

    if (raw != d->raw) {
        d->raw = raw;
        d->counter = 0;
    }

    if (d->raw != d->stable) {
        d->counter++;
        if (d->counter >= d->threshold) {
            d->stable = d->raw;
            d->changed = true;
            d->counter = 0;
        }
    } else {
        d->counter = 0;
    }

    return d->stable;
}

bool debounce_get(const debounce_t *d)
{
    if (!d) return false;
    return d->stable;
}

bool debounce_changed(const debounce_t *d)
{
    if (!d) return false;
    return d->changed;
}

bool debounce_rising(const debounce_t *d)
{
    if (!d) return false;
    return d->changed && d->stable;
}

bool debounce_falling(const debounce_t *d)
{
    if (!d) return false;
    return d->changed && !d->stable;
}
