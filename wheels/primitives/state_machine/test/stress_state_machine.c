// SPDX-License-Identifier: MIT
/*
 * stress_state_machine.c — long-running stress tests for state_machine
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_state_machine.c ../state_machine.c -o /tmp/stress_state_machine -lm
 *
 * Scenarios:
 *   1. 1M rapid transitions in a 4-state loop — verify counter and state
 *   2. Guard stress — guards that alternate allow/deny over 100K events
 *   3. No-transition events — 100K unhandled events must not corrupt state
 *   4. Entry/exit action count — verify actions fire exactly once per transition
 */

#include "test_harness.h"
#include "state_machine.h"

enum { ST_A = 0, ST_B, ST_C, ST_D, ST_COUNT };
enum { EV_NEXT = 0, EV_BACK, EV_NOOP };

static int g_action_count;
static int g_enter_count;
static int g_exit_count;
static bool g_guard_allow;

static void action_count(void *ctx) { (void)ctx; g_action_count++; }
static void enter_count(void *ctx) { (void)ctx; g_enter_count++; }
static void exit_count(void *ctx) { (void)ctx; g_exit_count++; }
static bool guard_toggle(void *ctx) { (void)ctx; return g_guard_allow; }

/* ---- tests ---------------------------------------------------------- */

TEST(rapid_loop_million_transitions)
{
    /* A -> B -> C -> D -> A cycle via EV_NEXT */
    static const sm_transition_t trans[] = {
        { ST_A, EV_NEXT, NULL, ST_B, NULL },
        { ST_B, EV_NEXT, NULL, ST_C, NULL },
        { ST_C, EV_NEXT, NULL, ST_D, NULL },
        { ST_D, EV_NEXT, NULL, ST_A, NULL },
    };
    sm_cfg_t cfg = {
        .transitions      = trans,
        .num_transitions  = 4,
        .state_defs       = NULL,
        .num_state_defs   = 0,
        .user_ctx         = NULL,
    };
    state_machine_t sm;
    state_machine_init(&sm, &cfg, ST_A);

    const int iterations = 1000000;
    int ok = 1;
    for (int i = 0; i < iterations; i++) {
        state_machine_process(&sm, EV_NEXT);
    }
    /* After 1M transitions from A: 1M % 4 = 0, back to A */
    ASSERT_EQ(ST_A, state_machine_current(&sm));
    ASSERT_EQ((uint32_t)iterations, state_machine_transition_count(&sm));
    (void)ok;
}

TEST(guard_alternation_stress)
{
    static const sm_transition_t trans[] = {
        { ST_A, EV_NEXT, guard_toggle, ST_B, action_count },
        { ST_B, EV_NEXT, NULL,         ST_A, action_count },
    };
    sm_cfg_t cfg = {
        .transitions      = trans,
        .num_transitions  = 2,
        .state_defs       = NULL,
        .num_state_defs   = 0,
        .user_ctx         = NULL,
    };
    state_machine_t sm;
    state_machine_init(&sm, &cfg, ST_A);

    g_action_count = 0;
    int ok = 1;
    int successful_transitions = 0;
    int guard_blocked = 0;

    for (int i = 0; i < 100000; i++) {
        g_guard_allow = (i % 3 == 0); /* allow every 3rd attempt from A */
        state_machine_err_t err = state_machine_process(&sm, EV_NEXT);
        if (err == STATE_MACHINE_OK) {
            successful_transitions++;
        } else {
            guard_blocked++;
        }
    }
    /* action fires exactly once per successful transition */
    ASSERT_EQ(successful_transitions, g_action_count);
    /* Both allowed and blocked must have occurred */
    ASSERT_TRUE(successful_transitions > 0);
    ASSERT_TRUE(guard_blocked > 0);
    (void)ok;
}

TEST(no_transition_events_no_corruption)
{
    static const sm_transition_t trans[] = {
        { ST_A, EV_NEXT, NULL, ST_B, NULL },
        { ST_B, EV_BACK, NULL, ST_A, NULL },
    };
    sm_cfg_t cfg = {
        .transitions      = trans,
        .num_transitions  = 2,
        .state_defs       = NULL,
        .num_state_defs   = 0,
        .user_ctx         = NULL,
    };
    state_machine_t sm;
    state_machine_init(&sm, &cfg, ST_A);

    /* Send 100K EV_NOOP events — no transition defined, state must not change */
    for (int i = 0; i < 100000; i++) {
        state_machine_err_t err = state_machine_process(&sm, EV_NOOP);
        ASSERT_EQ(STATE_MACHINE_ERR_NO_TRANSITION, err);
    }
    ASSERT_EQ(ST_A, state_machine_current(&sm));
    ASSERT_EQ(0u, state_machine_transition_count(&sm));
}

TEST(entry_exit_action_count)
{
    static const sm_state_def_t states[] = {
        { ST_A, enter_count, exit_count },
        { ST_B, enter_count, exit_count },
    };
    static const sm_transition_t trans[] = {
        { ST_A, EV_NEXT, NULL, ST_B, NULL },
        { ST_B, EV_NEXT, NULL, ST_A, NULL },
    };
    sm_cfg_t cfg = {
        .transitions      = trans,
        .num_transitions  = 2,
        .state_defs       = states,
        .num_state_defs   = 2,
        .user_ctx         = NULL,
    };
    state_machine_t sm;
    state_machine_init(&sm, &cfg, ST_A);

    g_enter_count = 0;
    g_exit_count = 0;
    const int transitions = 100000;
    for (int i = 0; i < transitions; i++) {
        state_machine_process(&sm, EV_NEXT);
    }
    /* Each transition fires one exit + one enter */
    ASSERT_EQ(transitions, g_enter_count);
    ASSERT_EQ(transitions, g_exit_count);
    /* Even number of transitions returns to start */
    ASSERT_EQ(ST_A, state_machine_current(&sm));
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress state_machine")
    RUN(rapid_loop_million_transitions);
    RUN(guard_alternation_stress);
    RUN(no_transition_events_no_corruption);
    RUN(entry_exit_action_count);
TEST_MAIN_END()
