/*
 * timer_wheel — hashed timing wheel for O(1) timeout management
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "timer_wheel.h"
#include <string.h>

/* Sentinel value for "no entry" in slot chains. */
#define TW_NONE UINT32_MAX

/* ---- init ---------------------------------------------------------- */

timer_wheel_err_t timer_wheel_init(timer_wheel_t *tw,
                                   timer_wheel_slot_t *slots,
                                   uint32_t num_slots,
                                   timer_wheel_entry_t *timers,
                                   uint32_t max_timers)
{
    if (!tw || !slots || !timers || num_slots == 0 || max_timers == 0)
    {
        return TIMER_WHEEL_ERR_PARAM;
    }

    tw->slots      = slots;
    tw->timers     = timers;
    tw->num_slots  = num_slots;
    tw->max_timers = max_timers;
    tw->current_pos = 0;
    tw->count      = 0;

    for (uint32_t i = 0; i < num_slots; i++)
    {
        slots[i].head = TW_NONE;
        slots[i].tail = TW_NONE;
    }

    memset(timers, 0, sizeof(timer_wheel_entry_t) * max_timers);

    return TIMER_WHEEL_OK;
}

/* ---- add ----------------------------------------------------------- */

timer_wheel_err_t timer_wheel_add(timer_wheel_t *tw,
                                  uint32_t id,
                                  uint32_t ticks,
                                  timer_wheel_cb_t callback,
                                  void *ctx)
{
    if (!tw || !callback || ticks == 0 || id >= tw->max_timers)
    {
        return TIMER_WHEEL_ERR_PARAM;
    }

    if (tw->timers[id].active)
    {
        return TIMER_WHEEL_ERR_FULL;
    }

    uint32_t target_slot      = (tw->current_pos + ticks) % tw->num_slots;
    uint32_t remaining_rounds = (ticks - 1) / tw->num_slots;

    timer_wheel_entry_t *e = &tw->timers[id];
    e->remaining_rounds = remaining_rounds;
    e->slot             = target_slot;
    e->callback         = callback;
    e->ctx              = ctx;
    e->active           = true;

    tw->count++;

    return TIMER_WHEEL_OK;
}

/* ---- cancel -------------------------------------------------------- */

timer_wheel_err_t timer_wheel_cancel(timer_wheel_t *tw, uint32_t id)
{
    if (!tw || id >= tw->max_timers)
    {
        return TIMER_WHEEL_ERR_PARAM;
    }

    timer_wheel_entry_t *e = &tw->timers[id];
    if (!e->active)
    {
        return TIMER_WHEEL_ERR_PARAM;
    }

    e->active   = false;
    e->callback = NULL;
    tw->count--;

    return TIMER_WHEEL_OK;
}

/* ---- tick ---------------------------------------------------------- */

void timer_wheel_tick(timer_wheel_t *tw)
{
    if (!tw)
    {
        return;
    }

    tw->current_pos = (tw->current_pos + 1) % tw->num_slots;

    uint32_t slot = tw->current_pos;

    /*
     * Scan all timer entries that belong to this slot.
     * For small max_timers (typical embedded use), a linear scan is
     * fast and avoids the complexity of per-slot linked lists.
     */
    for (uint32_t i = 0; i < tw->max_timers; i++)
    {
        timer_wheel_entry_t *e = &tw->timers[i];

        if (!e->active || e->slot != slot)
        {
            continue;
        }

        if (e->remaining_rounds > 0)
        {
            e->remaining_rounds--;
            continue;
        }

        /* Timer expired — fire callback, then deactivate */
        timer_wheel_cb_t cb  = e->callback;
        void            *ctx = e->ctx;

        e->active   = false;
        e->callback = NULL;
        tw->count--;

        cb(ctx);
    }
}
