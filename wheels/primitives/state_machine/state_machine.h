// SPDX-License-Identifier: MIT
/*
 * state_machine — table-driven finite state machine
 *
 * Layer:   L0
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Table-driven FSM with optional entry/exit actions per state and
 * guard conditions on transitions. Caller defines states, events,
 * and a static transition table. No heap, fully reentrant per-context.
 * Thread safety: not thread-safe. Callers must serialize access.
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATE_MACHINE_OK              =  0,
    STATE_MACHINE_ERR_PARAM       = -1,
    STATE_MACHINE_ERR_NO_TRANSITION = -2,
} state_machine_err_t;

typedef uint16_t sm_state_t;
typedef uint16_t sm_event_t;

typedef void (*sm_action_t)(void *user_ctx);
typedef bool (*sm_guard_t)(void *user_ctx);

typedef struct {
    sm_state_t  current_state;
    sm_event_t  event;
    sm_guard_t  guard;
    sm_state_t  next_state;
    sm_action_t action;
} sm_transition_t;

typedef struct {
    sm_state_t  id;
    sm_action_t on_enter;
    sm_action_t on_exit;
} sm_state_def_t;

typedef struct {
    const sm_transition_t *transitions;
    size_t                 num_transitions;
    const sm_state_def_t  *state_defs;
    size_t                 num_state_defs;
    void                  *user_ctx;
} sm_cfg_t;

typedef struct {
    sm_cfg_t    cfg;
    sm_state_t  current;
    uint32_t    transition_count;
} state_machine_t;

/** @thread_safe: no */
state_machine_err_t state_machine_init(state_machine_t *ctx, const sm_cfg_t *cfg,
                                       sm_state_t initial_state);

/** @thread_safe: no */
state_machine_err_t state_machine_process(state_machine_t *ctx, sm_event_t event);

/** @thread_safe: no */
sm_state_t state_machine_current(const state_machine_t *ctx);

/** @thread_safe: no */
uint32_t state_machine_transition_count(const state_machine_t *ctx);

/** @thread_safe: no */
state_machine_err_t state_machine_reset(state_machine_t *ctx, sm_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* STATE_MACHINE_H */
