// SPDX-License-Identifier: MIT
/*
 * task_watchdog — passive tick-driven task health monitor
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */

#include "task_watchdog.h"
#include <string.h>

/* ---- internal helper --------------------------------------------------- */

static task_wd_entry_t *find_task(task_watchdog_t *wd, const char *name)
{
    for (uint8_t i = 0; i < wd->count; i++)
    {
        if (wd->tasks[i].name && strcmp(wd->tasks[i].name, name) == 0)
        {
            return &wd->tasks[i];
        }
    }
    return NULL;
}

static const task_wd_entry_t *find_task_const(const task_watchdog_t *wd,
                                               const char *name)
{
    for (uint8_t i = 0; i < wd->count; i++)
    {
        if (wd->tasks[i].name && strcmp(wd->tasks[i].name, name) == 0)
        {
            return &wd->tasks[i];
        }
    }
    return NULL;
}

/* ---- public API -------------------------------------------------------- */

task_wd_err_t task_watchdog_init(task_watchdog_t *wd)
{
    if (!wd)
    {
        return TASK_WD_ERR_PARAM;
    }
    memset(wd, 0, sizeof(*wd));
    return TASK_WD_OK;
}

task_wd_err_t task_watchdog_register(task_watchdog_t *wd, const char *name,
                                     uint32_t timeout_ticks)
{
    if (!wd || !name || timeout_ticks == 0)
    {
        return TASK_WD_ERR_PARAM;
    }
    if (wd->count >= TASK_WD_MAX_TASKS)
    {
        return TASK_WD_ERR_FULL;
    }

    task_wd_entry_t *e = &wd->tasks[wd->count];
    e->name      = name;
    e->timeout   = timeout_ticks;
    e->last_feed = wd->tick_count;
    e->status    = TASK_WD_STATUS_OK;
    e->enabled   = true;
    wd->count++;

    return TASK_WD_OK;
}

task_wd_err_t task_watchdog_feed(task_watchdog_t *wd, const char *name)
{
    if (!wd || !name)
    {
        return TASK_WD_ERR_PARAM;
    }

    task_wd_entry_t *e = find_task(wd, name);
    if (!e)
    {
        return TASK_WD_ERR_NOT_FOUND;
    }

    e->last_feed = wd->tick_count;
    e->status    = TASK_WD_STATUS_OK;
    return TASK_WD_OK;
}

void task_watchdog_check(task_watchdog_t *wd)
{
    if (!wd)
    {
        return;
    }

    for (uint8_t i = 0; i < wd->count; i++)
    {
        task_wd_entry_t *e = &wd->tasks[i];
        if (!e->enabled)
        {
            continue;
        }
        if (e->status == TASK_WD_STATUS_EXPIRED)
        {
            /* already reported — skip until re-fed */
            continue;
        }

        uint32_t elapsed = wd->tick_count - e->last_feed;
        if (elapsed >= e->timeout)
        {
            e->status = TASK_WD_STATUS_EXPIRED;
            if (wd->on_expired)
            {
                wd->on_expired(wd->expired_ctx, e->name, e->last_feed);
            }
        }
    }
}

void task_watchdog_tick(task_watchdog_t *wd)
{
    if (!wd)
    {
        return;
    }
    wd->tick_count++;
}

void task_watchdog_set_callback(task_watchdog_t *wd, task_wd_callback_t cb,
                                void *ctx)
{
    if (!wd)
    {
        return;
    }
    wd->on_expired  = cb;
    wd->expired_ctx = ctx;
}

task_wd_err_t task_watchdog_enable(task_watchdog_t *wd, const char *name)
{
    if (!wd || !name)
    {
        return TASK_WD_ERR_PARAM;
    }

    task_wd_entry_t *e = find_task(wd, name);
    if (!e)
    {
        return TASK_WD_ERR_NOT_FOUND;
    }

    e->enabled   = true;
    e->last_feed = wd->tick_count;
    e->status    = TASK_WD_STATUS_OK;
    return TASK_WD_OK;
}

task_wd_err_t task_watchdog_disable(task_watchdog_t *wd, const char *name)
{
    if (!wd || !name)
    {
        return TASK_WD_ERR_PARAM;
    }

    task_wd_entry_t *e = find_task(wd, name);
    if (!e)
    {
        return TASK_WD_ERR_NOT_FOUND;
    }

    e->enabled = false;
    e->status  = TASK_WD_STATUS_DISABLED;
    return TASK_WD_OK;
}

task_wd_status_t task_watchdog_status(const task_watchdog_t *wd,
                                      const char *name)
{
    if (!wd || !name)
    {
        return TASK_WD_STATUS_DISABLED;
    }

    const task_wd_entry_t *e = find_task_const(wd, name);
    if (!e)
    {
        return TASK_WD_STATUS_DISABLED;
    }
    return e->status;
}

uint8_t task_watchdog_count(const task_watchdog_t *wd)
{
    if (!wd)
    {
        return 0;
    }
    return wd->count;
}

uint32_t task_watchdog_expired_count(const task_watchdog_t *wd)
{
    if (!wd)
    {
        return 0;
    }

    uint32_t n = 0;
    for (uint8_t i = 0; i < wd->count; i++)
    {
        if (wd->tasks[i].status == TASK_WD_STATUS_EXPIRED)
        {
            n++;
        }
    }
    return n;
}
