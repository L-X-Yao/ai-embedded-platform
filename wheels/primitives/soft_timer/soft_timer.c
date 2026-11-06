// SPDX-License-Identifier: MIT
/*
 * soft_timer — tick-driven software timer manager
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "soft_timer.h"
#include "contract.h"
#include <string.h>

/* count is a high-water mark: destroy clears a slot and leaves count alone,
 * so a slot only holds a timer while it has a callback. */
static bool slot_in_use(const soft_timer_mgr_t *mgr, int id)
{
    return mgr && id >= 0 && (size_t)id < mgr->count &&
           mgr->timers[id].callback != NULL;
}

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

    /* Search freed slots (callback==NULL) first, before checking count.
     * count is a high-water mark that never decrements, so we must look
     * for reusable gaps before concluding the pool is full. */
    int id = -1;
    for (size_t i = 0; i < mgr->count; i++)
    {
        if (mgr->timers[i].callback == NULL)
        {
            id = (int)i;
            break;
        }
    }
    if (id < 0)
    {
        if (mgr->count >= SOFT_TIMER_MAX)
        {
            return SOFT_TIMER_ERR_FULL;
        }
        id = (int)mgr->count;
        mgr->count++;
    }

    soft_timer_entry_t *t = &mgr->timers[id];
    t->period = period_ticks;
    t->remaining = period_ticks;
    t->callback = cb;
    t->user_ctx = user_ctx;
    t->mode = mode;
    t->active = false;

    ENSURE(id >= 0 && (size_t)id < SOFT_TIMER_MAX);
    INVARIANT(mgr->count <= SOFT_TIMER_MAX);
    return id;
}

soft_timer_err_t soft_timer_destroy(soft_timer_mgr_t *mgr, int id)
{
    if (!mgr || id < 0 || (size_t)id >= mgr->count)
    {
        return SOFT_TIMER_ERR_PARAM;
    }
    memset(&mgr->timers[id], 0, sizeof(soft_timer_entry_t));
    return SOFT_TIMER_OK;
}

soft_timer_err_t soft_timer_start(soft_timer_mgr_t *mgr, int id)
{
    if (!slot_in_use(mgr, id))
    {
        return SOFT_TIMER_ERR_PARAM;
    }
    /* A fired one-shot sits at 0, which tick would take as due at once.
     * A stopped timer keeps what it has left. */
    if (mgr->timers[id].remaining == 0)
    {
        mgr->timers[id].remaining = mgr->timers[id].period;
    }
    mgr->timers[id].active = true;
    return SOFT_TIMER_OK;
}

soft_timer_err_t soft_timer_stop(soft_timer_mgr_t *mgr, int id)
{
    if (!slot_in_use(mgr, id))
    {
        return SOFT_TIMER_ERR_PARAM;
    }
    mgr->timers[id].active = false;
    return SOFT_TIMER_OK;
}

soft_timer_err_t soft_timer_reset(soft_timer_mgr_t *mgr, int id)
{
    if (!slot_in_use(mgr, id))
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

    size_t n = mgr->count;
    for (size_t i = 0; i < n; i++)
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
            /* Settle the timer first: the callback may re-arm, stop or
             * destroy it, and nothing after the call may undo that. */
            if (t->mode == SOFT_TIMER_REPEAT)
            {
                t->remaining = t->period;
            }
            else
            {
                t->active = false;
            }

            t->callback(t->user_ctx);
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
