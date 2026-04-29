/*
 * soft_timer — tick-driven software timer manager
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "soft_timer.h"
#include <string.h>

soft_timer_err_t soft_timer_init(soft_timer_mgr_t *mgr)
{
    if (!mgr)
    {
        return SOFT_TIMER_ERR_PARAM;
    }
    memset(mgr, 0, sizeof(*mgr));
    return SOFT_TIMER_OK;
}

int soft_timer_create(soft_timer_mgr_t *mgr, uint32_t period_ticks,
                      soft_timer_mode_t mode, soft_timer_cb_t cb,
                      void *user_ctx)
{
    if (!mgr || period_ticks == 0 || !cb)
    {
        return SOFT_TIMER_ERR_PARAM;
    }
    if (mgr->count >= SOFT_TIMER_MAX)
    {
        return SOFT_TIMER_ERR_FULL;
    }

    int id = (int)mgr->count;
    soft_timer_entry_t *t = &mgr->timers[id];
    t->period = period_ticks;
    t->remaining = period_ticks;
    t->callback = cb;
    t->user_ctx = user_ctx;
    t->mode = mode;
    t->active = false;
    mgr->count++;

    return id;
}

soft_timer_err_t soft_timer_start(soft_timer_mgr_t *mgr, int id)
{
    if (!mgr || id < 0 || (size_t)id >= mgr->count)
    {
        return SOFT_TIMER_ERR_PARAM;
    }
    mgr->timers[id].active = true;
    return SOFT_TIMER_OK;
}

soft_timer_err_t soft_timer_stop(soft_timer_mgr_t *mgr, int id)
{
    if (!mgr || id < 0 || (size_t)id >= mgr->count)
    {
        return SOFT_TIMER_ERR_PARAM;
    }
    mgr->timers[id].active = false;
    return SOFT_TIMER_OK;
}

soft_timer_err_t soft_timer_reset(soft_timer_mgr_t *mgr, int id)
{
    if (!mgr || id < 0 || (size_t)id >= mgr->count)
    {
        return SOFT_TIMER_ERR_PARAM;
    }
    mgr->timers[id].remaining = mgr->timers[id].period;
    return SOFT_TIMER_OK;
}

bool soft_timer_is_active(const soft_timer_mgr_t *mgr, int id)
{
    if (!mgr || id < 0 || (size_t)id >= mgr->count)
    {
        return false;
    }
    return mgr->timers[id].active;
}

void soft_timer_tick(soft_timer_mgr_t *mgr)
{
    if (!mgr)
    {
        return;
    }

    mgr->tick_count++;

    for (size_t i = 0; i < mgr->count; i++)
    {
        soft_timer_entry_t *t = &mgr->timers[i];
        if (!t->active)
        {
            continue;
        }

        if (t->remaining > 0)
        {
            t->remaining--;
        }

        if (t->remaining == 0)
        {
            t->callback(t->user_ctx);

            if (t->mode == SOFT_TIMER_REPEAT)
            {
                t->remaining = t->period;
            }
            else
            {
                t->active = false;
            }
        }
    }
}

uint32_t soft_timer_get_tick(const soft_timer_mgr_t *mgr)
{
    if (!mgr)
    {
        return 0;
    }
    return mgr->tick_count;
}
