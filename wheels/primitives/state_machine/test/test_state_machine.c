// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "state_machine.h"
/* --- Test FSM: traffic light --- */

enum { S_RED = 0, S_GREEN, S_YELLOW };
enum { E_TIMER = 0, E_EMERGENCY };

static int enter_count[3];
static int exit_count[3];
static int action_count;

static void reset_counters(void)
{
    memset(enter_count, 0, sizeof(enter_count));
    memset(exit_count, 0, sizeof(exit_count));
    action_count = 0;
}

static void on_enter_red(void *ctx)    { (void)ctx; enter_count[S_RED]++; }
static void on_exit_red(void *ctx)     { (void)ctx; exit_count[S_RED]++; }
static void on_enter_green(void *ctx)  { (void)ctx; enter_count[S_GREEN]++; }
static void on_exit_green(void *ctx)   { (void)ctx; exit_count[S_GREEN]++; }
static void on_enter_yellow(void *ctx) { (void)ctx; enter_count[S_YELLOW]++; }
static void on_exit_yellow(void *ctx)  { (void)ctx; exit_count[S_YELLOW]++; }
static void action_switch(void *ctx)   { (void)ctx; action_count++; }

static bool guard_true(void *ctx)  { (void)ctx; return true; }
static bool guard_false(void *ctx) { (void)ctx; return false; }

static const sm_state_def_t state_defs[] = {
    { S_RED,    on_enter_red,    on_exit_red },
    { S_GREEN,  on_enter_green,  on_exit_green },
    { S_YELLOW, on_enter_yellow, on_exit_yellow },
};

static const sm_transition_t transitions[] = {
    { S_RED,    E_TIMER,     NULL, S_GREEN,  action_switch },
    { S_GREEN,  E_TIMER,     NULL, S_YELLOW, action_switch },
    { S_YELLOW, E_TIMER,     NULL, S_RED,    action_switch },
    { S_GREEN,  E_EMERGENCY, NULL, S_RED,    action_switch },
    { S_YELLOW, E_EMERGENCY, NULL, S_RED,    action_switch },
};

static state_machine_t sm;

static void setup_traffic_light(void)
{
    reset_counters();
    sm_cfg_t cfg = {
        .transitions = transitions,
        .num_transitions = sizeof(transitions) / sizeof(transitions[0]),
        .state_defs = state_defs,
        .num_state_defs = sizeof(state_defs) / sizeof(state_defs[0]),
        .user_ctx = NULL,
    };
    state_machine_init(&sm, &cfg, S_RED);
}

/* --- Tests --- */

TEST(test_init)
{
    setup_traffic_light();
    ASSERT_EQ(state_machine_current(&sm), S_RED);
    ASSERT_EQ(state_machine_transition_count(&sm), 0);
    ASSERT_EQ(enter_count[S_RED], 1);
}

TEST(test_null_safety)
{
    sm_cfg_t cfg = {0};
    ASSERT_EQ(state_machine_init(NULL, &cfg, 0), STATE_MACHINE_ERR_PARAM);
    ASSERT_EQ(state_machine_init(&sm, NULL, 0), STATE_MACHINE_ERR_PARAM);
    ASSERT_EQ(state_machine_process(NULL, 0), STATE_MACHINE_ERR_PARAM);
    ASSERT_EQ(state_machine_current(NULL), 0);
    ASSERT_EQ(state_machine_transition_count(NULL), 0);
    ASSERT_EQ(state_machine_reset(NULL, 0), STATE_MACHINE_ERR_PARAM);
}

TEST(test_basic_transition)
{
    setup_traffic_light();
    state_machine_err_t err = state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(err, STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_current(&sm), S_GREEN);
    ASSERT_EQ(state_machine_transition_count(&sm), 1);
}

TEST(test_full_cycle)
{
    setup_traffic_light();
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_GREEN);
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_YELLOW);
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_RED);
    ASSERT_EQ(state_machine_transition_count(&sm), 3);
}

TEST(test_entry_exit_actions)
{
    setup_traffic_light();

    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(exit_count[S_RED], 1);
    ASSERT_EQ(enter_count[S_GREEN], 1);
    ASSERT_EQ(action_count, 1);
}

TEST(test_emergency)
{
    setup_traffic_light();
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_GREEN);

    state_machine_process(&sm, E_EMERGENCY);
    ASSERT_EQ(state_machine_current(&sm), S_RED);
}

TEST(test_no_transition)
{
    setup_traffic_light();
    state_machine_err_t err = state_machine_process(&sm, E_EMERGENCY);
    ASSERT_EQ(err, STATE_MACHINE_ERR_NO_TRANSITION);
    ASSERT_EQ(state_machine_current(&sm), S_RED);
    ASSERT_EQ(state_machine_transition_count(&sm), 0);
}

TEST(test_unknown_event)
{
    setup_traffic_light();
    state_machine_err_t err = state_machine_process(&sm, 99);
    ASSERT_EQ(err, STATE_MACHINE_ERR_NO_TRANSITION);
    ASSERT_EQ(state_machine_current(&sm), S_RED);
}

TEST(test_guard_pass)
{
    reset_counters();
    static const sm_transition_t guarded[] = {
        { S_RED, E_TIMER, guard_true, S_GREEN, NULL },
    };
    sm_cfg_t cfg = {
        .transitions = guarded,
        .num_transitions = 1,
        .state_defs = NULL,
        .num_state_defs = 0,
        .user_ctx = NULL,
    };
    state_machine_init(&sm, &cfg, S_RED);
    state_machine_err_t err = state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(err, STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_current(&sm), S_GREEN);
}

TEST(test_guard_block)
{
    reset_counters();
    static const sm_transition_t guarded[] = {
        { S_RED, E_TIMER, guard_false, S_GREEN, NULL },
    };
    sm_cfg_t cfg = {
        .transitions = guarded,
        .num_transitions = 1,
        .state_defs = NULL,
        .num_state_defs = 0,
        .user_ctx = NULL,
    };
    state_machine_init(&sm, &cfg, S_RED);
    state_machine_err_t err = state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(err, STATE_MACHINE_ERR_NO_TRANSITION);
    ASSERT_EQ(state_machine_current(&sm), S_RED);
}

TEST(test_self_transition)
{
    reset_counters();
    static const sm_transition_t self[] = {
        { S_RED, E_TIMER, NULL, S_RED, action_switch },
    };
    sm_cfg_t cfg = {
        .transitions = self,
        .num_transitions = 1,
        .state_defs = state_defs,
        .num_state_defs = sizeof(state_defs) / sizeof(state_defs[0]),
        .user_ctx = NULL,
    };
    state_machine_init(&sm, &cfg, S_RED);
    reset_counters();

    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_RED);
    ASSERT_EQ(action_count, 1);
    ASSERT_EQ(enter_count[S_RED], 0);
    ASSERT_EQ(exit_count[S_RED], 0);
}

TEST(test_reset)
{
    setup_traffic_light();
    state_machine_process(&sm, E_TIMER);
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_YELLOW);

    state_machine_reset(&sm, S_RED);
    ASSERT_EQ(state_machine_current(&sm), S_RED);
    ASSERT_EQ(state_machine_transition_count(&sm), 0);
}

TEST(test_no_state_defs)
{
    reset_counters();
    sm_cfg_t cfg = {
        .transitions = transitions,
        .num_transitions = sizeof(transitions) / sizeof(transitions[0]),
        .state_defs = NULL,
        .num_state_defs = 0,
        .user_ctx = NULL,
    };
    state_machine_init(&sm, &cfg, S_RED);
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_GREEN);
    ASSERT_EQ(enter_count[S_GREEN], 0);
}

TEST(test_no_transitions)
{
    sm_cfg_t cfg = {
        .transitions = NULL,
        .num_transitions = 0,
        .state_defs = NULL,
        .num_state_defs = 0,
        .user_ctx = NULL,
    };
    state_machine_init(&sm, &cfg, S_RED);
    state_machine_err_t err = state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(err, STATE_MACHINE_ERR_NO_TRANSITION);
}

TEST(test_multiple_cycles)
{
    setup_traffic_light();
    for (int i = 0; i < 9; i++)
    {
        state_machine_process(&sm, E_TIMER);
    }
    ASSERT_EQ(state_machine_current(&sm), S_RED);
    ASSERT_EQ(state_machine_transition_count(&sm), 9);
}

TEST_MAIN("state_machine")
    RUN(test_init);
    RUN(test_null_safety);
    RUN(test_basic_transition);
    RUN(test_full_cycle);
    RUN(test_entry_exit_actions);
    RUN(test_emergency);
    RUN(test_no_transition);
    RUN(test_unknown_event);
    RUN(test_guard_pass);
    RUN(test_guard_block);
    RUN(test_self_transition);
    RUN(test_reset);
    RUN(test_no_state_defs);
    RUN(test_no_transitions);
    RUN(test_multiple_cycles);
TEST_MAIN_END()

