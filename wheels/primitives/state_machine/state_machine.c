/*
 * state_machine — table-driven finite state machine
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:    (none)
 */

#include "state_machine.h"
#include <stddef.h>
#include <limits.h>

static const sm_state_def_t *find_state_def(const state_machine_t *ctx,
                                            sm_state_t id)
{
    if (!ctx->cfg.state_defs)
    {
        return NULL;
    }
    for (size_t i = 0; i < ctx->cfg.num_state_defs; i++)
    {
        if (ctx->cfg.state_defs[i].id == id)
        {
            return &ctx->cfg.state_defs[i];
        }
    }
    return NULL;
}

state_machine_err_t state_machine_init(state_machine_t *ctx, const sm_cfg_t *cfg,
                                       sm_state_t initial_state)
{
    if (!ctx || !cfg)
    {
        return STATE_MACHINE_ERR_PARAM;
    }
    if (!cfg->transitions && cfg->num_transitions > 0)
    {
        return STATE_MACHINE_ERR_PARAM;
    }
    ctx->cfg = *cfg;
    ctx->current = initial_state;
    ctx->transition_count = 0;

    const sm_state_def_t *def = find_state_def(ctx, initial_state);
    if (def && def->on_enter)
    {
        def->on_enter(ctx->cfg.user_ctx);
    }

    return STATE_MACHINE_OK;
}

state_machine_err_t state_machine_process(state_machine_t *ctx, sm_event_t event)
{
    if (!ctx)
    {
        return STATE_MACHINE_ERR_PARAM;
    }

    for (size_t i = 0; i < ctx->cfg.num_transitions; i++)
    {
        const sm_transition_t *tr = &ctx->cfg.transitions[i];

        if (tr->current_state != ctx->current || tr->event != event)
        {
            continue;
        }

        if (tr->guard && !tr->guard(ctx->cfg.user_ctx))
        {
            continue;
        }

        sm_state_t old_state = ctx->current;
        sm_state_t new_state = tr->next_state;

        if (old_state != new_state)
        {
            const sm_state_def_t *old_def = find_state_def(ctx, old_state);
            if (old_def && old_def->on_exit)
            {
                old_def->on_exit(ctx->cfg.user_ctx);
            }
        }

        if (tr->action)
        {
            tr->action(ctx->cfg.user_ctx);
        }

        ctx->current = new_state;
        if (ctx->transition_count < UINT32_MAX) ctx->transition_count++;

        if (old_state != new_state)
        {
            const sm_state_def_t *new_def = find_state_def(ctx, new_state);
            if (new_def && new_def->on_enter)
            {
                new_def->on_enter(ctx->cfg.user_ctx);
            }
        }

        return STATE_MACHINE_OK;
    }

    return STATE_MACHINE_ERR_NO_TRANSITION;
}

sm_state_t state_machine_current(const state_machine_t *ctx)
{
    if (!ctx)
    {
        return 0;
    }
    return ctx->current;
}

uint32_t state_machine_transition_count(const state_machine_t *ctx)
{
    if (!ctx)
    {
        return 0;
    }
    return ctx->transition_count;
}

state_machine_err_t state_machine_reset(state_machine_t *ctx, sm_state_t state)
{
    if (!ctx)
    {
        return STATE_MACHINE_ERR_PARAM;
    }
    ctx->current = state;
    ctx->transition_count = 0;

    const sm_state_def_t *def = find_state_def(ctx, state);
    if (def && def->on_enter)
    {
        def->on_enter(ctx->cfg.user_ctx);
    }

    return STATE_MACHINE_OK;
}
