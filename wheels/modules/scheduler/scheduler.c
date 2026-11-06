// SPDX-License-Identifier: MIT
/*
 * scheduler -- cooperative task scheduler for bare-metal / single-threaded
 *
 * Layer:   L3
 * Status:  candidate
 * Deps:      none
 * Instances: multi
 */
#include "scheduler.h"
#include "contract.h"
#include <string.h>

/* ----- internal: find task index by name ----- */

static int find_task(const scheduler_t *s, const char *name)
{
    for (uint8_t i = 0; i < s->count; i++) {
        if (strcmp(s->tasks[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

/* ----- public API ----- */

sched_err_t scheduler_init(scheduler_t *s)
{
    if (!s) return SCHED_ERR_PARAM;
    memset(s, 0, sizeof(*s));
    ENSURE(s->count == 0 && s->tick == 0);
    return SCHED_OK;
}

sched_err_t scheduler_add(scheduler_t *s, const char *name,
                          sched_task_fn fn, void *ctx,
                          uint32_t interval_ticks)
{
    if (!s || !name || !fn) return SCHED_ERR_PARAM;
    if (s->count >= SCHED_MAX_TASKS) return SCHED_ERR_FULL;

    sched_task_t *t = &s->tasks[s->count];
    t->name           = name;
    t->fn             = fn;
    t->ctx            = ctx;
    t->interval_ticks = interval_ticks;
    t->last_run       = s->tick;
    t->enabled        = true;
    t->run_count      = 0;
    t->overrun_count  = 0;
    s->count++;
    INVARIANT(s->count <= SCHED_MAX_TASKS);
    return SCHED_OK;
}

void scheduler_tick(scheduler_t *s)
{
    if (!s) return;
    s->tick++;
}

void scheduler_run(scheduler_t *s)
{
    if (!s) return;

    /* In s, not a local: scheduler_remove moves it when a task removes a task */
    s->next = 0;
    while (s->next < s->count) {
        sched_task_t *t = &s->tasks[s->next];
        s->next++;
        if (!t->enabled) continue;

        uint32_t elapsed = s->tick - t->last_run;
        if (elapsed >= t->interval_ticks) {
            /* detect overrun: missed more than one full interval */
            if (t->interval_ticks > 0 && elapsed > t->interval_ticks) {
                if (t->overrun_count < UINT32_MAX) t->overrun_count++;
            }
            t->last_run = s->tick;
            if (t->run_count < UINT32_MAX) t->run_count++;
            t->fn(t->ctx);  /* it may remove itself: t is not ours after this */
        }
    }
}

sched_err_t scheduler_enable(scheduler_t *s, const char *name)
{
    if (!s || !name) return SCHED_ERR_PARAM;
    int idx = find_task(s, name);
    if (idx < 0) return SCHED_ERR_PARAM;
    s->tasks[idx].enabled = true;
    return SCHED_OK;
}

sched_err_t scheduler_disable(scheduler_t *s, const char *name)
{
    if (!s || !name) return SCHED_ERR_PARAM;
    int idx = find_task(s, name);
    if (idx < 0) return SCHED_ERR_PARAM;
    s->tasks[idx].enabled = false;
    return SCHED_OK;
}

sched_err_t scheduler_remove(scheduler_t *s, const char *name)
{
    if (!s || !name) return SCHED_ERR_PARAM;
    int idx = find_task(s, name);
    if (idx < 0) return SCHED_ERR_PARAM;

    /* shift remaining tasks down */
    for (uint8_t i = (uint8_t)idx; i < s->count - 1; i++) {
        s->tasks[i] = s->tasks[i + 1];
    }
    s->count--;
    memset(&s->tasks[s->count], 0, sizeof(sched_task_t));

    /* The tasks after idx moved down a slot, so scheduler_run's place did too */
    if ((uint8_t)idx < s->next) s->next--;
    return SCHED_OK;
}

uint8_t scheduler_count(const scheduler_t *s)
{
    if (!s) return 0;
    return s->count;
}

uint32_t scheduler_get_run_count(const scheduler_t *s, const char *name)
{
    if (!s || !name) return 0;
    int idx = find_task(s, name);
    if (idx < 0) return 0;
    return s->tasks[idx].run_count;
}
