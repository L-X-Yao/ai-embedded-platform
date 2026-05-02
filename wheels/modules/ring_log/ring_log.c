// SPDX-License-Identifier: MIT
/*
 * ring_log — ring buffer structured log storage with read-back
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:    (none)
 */

#include "ring_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

ring_log_err_t ring_log_init(ring_log_t *rl, ring_log_level_t min_level)
{
    if (!rl) {
        return RING_LOG_ERR_PARAM;
    }
    memset(rl, 0, sizeof(*rl));
    rl->min_level = min_level;
    return RING_LOG_OK;
}

ring_log_err_t ring_log_write(ring_log_t *rl, ring_log_level_t level,
                              uint32_t timestamp, const char *fmt, ...)
{
    if (!rl || !fmt) {
        return RING_LOG_ERR_PARAM;
    }
    if (level < rl->min_level) {
        rl->dropped++;
        return RING_LOG_ERR_FILTERED;
    }

    ring_log_entry_t *slot = &rl->entries[rl->head];
    slot->timestamp = timestamp;
    slot->level     = level;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(slot->message, sizeof(slot->message), fmt, ap);
    va_end(ap);

    rl->head = (uint16_t)((rl->head + 1u) % RING_LOG_MAX_ENTRIES);
    if (rl->count < RING_LOG_MAX_ENTRIES) {
        rl->count++;
    }
    rl->total_logged++;
    return RING_LOG_OK;
}

ring_log_err_t ring_log_read(const ring_log_t *rl, uint16_t index,
                             ring_log_entry_t *entry)
{
    if (!rl || !entry) {
        return RING_LOG_ERR_PARAM;
    }
    if (rl->count == 0) {
        return RING_LOG_ERR_EMPTY;
    }
    if (index >= rl->count) {
        return RING_LOG_ERR_PARAM;
    }
    /* index 0 = newest (most recently written) */
    uint16_t pos = (uint16_t)((rl->head + RING_LOG_MAX_ENTRIES - 1u - index)
                              % RING_LOG_MAX_ENTRIES);
    *entry = rl->entries[pos];
    return RING_LOG_OK;
}

ring_log_err_t ring_log_read_oldest(const ring_log_t *rl, uint16_t nth,
                                    ring_log_entry_t *entry)
{
    if (!rl || !entry) {
        return RING_LOG_ERR_PARAM;
    }
    if (rl->count == 0) {
        return RING_LOG_ERR_EMPTY;
    }
    if (nth >= rl->count) {
        return RING_LOG_ERR_PARAM;
    }
    /* oldest entry is at (head - count) mod MAX */
    uint16_t oldest = (uint16_t)((rl->head + RING_LOG_MAX_ENTRIES - rl->count)
                                 % RING_LOG_MAX_ENTRIES);
    uint16_t pos = (uint16_t)((oldest + nth) % RING_LOG_MAX_ENTRIES);
    *entry = rl->entries[pos];
    return RING_LOG_OK;
}

uint16_t ring_log_count(const ring_log_t *rl)
{
    return rl ? rl->count : 0;
}

uint32_t ring_log_total(const ring_log_t *rl)
{
    return rl ? rl->total_logged : 0;
}

uint32_t ring_log_dropped(const ring_log_t *rl)
{
    return rl ? rl->dropped : 0;
}

void ring_log_clear(ring_log_t *rl)
{
    if (!rl) {
        return;
    }
    ring_log_level_t saved = rl->min_level;
    memset(rl, 0, sizeof(*rl));
    rl->min_level = saved;
}

ring_log_err_t ring_log_set_level(ring_log_t *rl, ring_log_level_t level)
{
    if (!rl) {
        return RING_LOG_ERR_PARAM;
    }
    rl->min_level = level;
    return RING_LOG_OK;
}
