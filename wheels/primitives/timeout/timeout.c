// SPDX-License-Identifier: MIT
/*
 * timeout — deadline-based timeout tracking
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */
#include "timeout.h"

void timeout_set(timeout_t *t, uint32_t now, uint32_t duration)
{
    if (!t) return;
    t->start = now;
    t->duration = duration;
}

bool timeout_expired(const timeout_t *t, uint32_t now)
{
    if (!t) return true;
    return (now - t->start) >= t->duration;
}

uint32_t timeout_remaining(const timeout_t *t, uint32_t now)
{
    if (!t) return 0;
    uint32_t elapsed = now - t->start;
    if (elapsed >= t->duration)
        return 0;
    return t->duration - elapsed;
}

uint32_t timeout_elapsed(const timeout_t *t, uint32_t now)
{
    if (!t) return 0;
    return now - t->start;
}

void timeout_restart(timeout_t *t, uint32_t now)
{
    if (!t) return;
    t->start = now;
}

void timeout_extend(timeout_t *t, uint32_t extra)
{
    if (!t) return;
    if (extra > UINT32_MAX - t->duration) {
        t->duration = UINT32_MAX;
    } else {
        t->duration += extra;
    }
}
