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

/* --- Calls made from inside a callback --- */

enum { N_A = 0, N_B, N_C, N_STATES };
enum { NE_GO = 0, NE_NEXT };

static state_machine_t     nest_sm;
static state_machine_err_t nest_rc;   /* what the call from the callback got */
static int nest_enter[N_STATES];
static int nest_exit[N_STATES];

static void post_next(void *ctx)
{
    (void)ctx;
    nest_rc = state_machine_process(&nest_sm, NE_NEXT);
}

static void reset_to_c(void *ctx)
{
    (void)ctx;
    nest_rc = state_machine_reset(&nest_sm, N_C);
}

static bool guard_posts_next(void *ctx)
{
    post_next(ctx);
    return true;
}

static void enter_a(void *ctx) { (void)ctx; nest_enter[N_A]++; }
static void exit_a(void *ctx)  { (void)ctx; nest_exit[N_A]++; }
static void enter_b(void *ctx) { (void)ctx; nest_enter[N_B]++; }
static void exit_b(void *ctx)  { (void)ctx; nest_exit[N_B]++; }
static void enter_c(void *ctx) { (void)ctx; nest_enter[N_C]++; }
static void exit_c(void *ctx)  { (void)ctx; nest_exit[N_C]++; }
static void enter_b_and_post(void *ctx) { enter_b(ctx); post_next(ctx); }
static void exit_a_and_post(void *ctx)  { exit_a(ctx); post_next(ctx); }

static const sm_state_def_t nest_defs[] = {
    { N_A, enter_a, exit_a },
    { N_B, enter_b, exit_b },
    { N_C, enter_c, exit_c },
};

static const sm_state_def_t nest_defs_enter_posts[] = {
    { N_A, enter_a, exit_a },
    { N_B, enter_b_and_post, exit_b },
    { N_C, enter_c, exit_c },
};

static const sm_state_def_t nest_defs_exit_posts[] = {
    { N_A, enter_a, exit_a_and_post },
    { N_B, enter_b, exit_b },
    { N_C, enter_c, exit_c },
};

static const sm_transition_t nest_plain[] = {
    { N_A, NE_GO,   NULL, N_B, NULL },
    { N_A, NE_NEXT, NULL, N_C, NULL },
    { N_B, NE_NEXT, NULL, N_C, NULL },
};

static const sm_transition_t nest_action_posts[] = {
    { N_A, NE_GO,   NULL, N_B, post_next },
    { N_A, NE_NEXT, NULL, N_C, NULL },
    { N_B, NE_NEXT, NULL, N_C, NULL },
};

static const sm_transition_t nest_guard_posts[] = {
    { N_A, NE_GO,   guard_posts_next, N_B, NULL },
    { N_A, NE_NEXT, NULL, N_C, NULL },
    { N_B, NE_NEXT, NULL, N_C, NULL },
};

static const sm_transition_t nest_action_resets[] = {
    { N_A, NE_GO,   NULL, N_B, reset_to_c },
    { N_B, NE_NEXT, NULL, N_C, NULL },
};

static const sm_transition_t nest_guard_blocks[] = {
    { N_A, NE_GO,   guard_false, N_B, NULL },
    { N_A, NE_NEXT, NULL, N_C, NULL },
};

static state_machine_err_t nest_setup(const sm_transition_t *tr, size_t n_tr,
                                      const sm_state_def_t *defs,
                                      sm_state_t initial)
{
    sm_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.transitions     = tr;
    cfg.num_transitions = n_tr;
    cfg.state_defs      = defs;
    cfg.num_state_defs  = N_STATES;

    memset(nest_enter, 0, sizeof(nest_enter));
    memset(nest_exit, 0, sizeof(nest_exit));
    nest_rc = STATE_MACHINE_OK;
    return state_machine_init(&nest_sm, &cfg, initial);
}

#define NEST_TABLE(t) (t), (sizeof(t) / sizeof((t)[0]))

/* After a refused call the machine must have made exactly the one
 * transition A to B, and must then take the next event normally. */
TEST(test_process_from_action_is_refused)
{
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_action_posts), nest_defs, N_A),
              STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_process(&nest_sm, NE_GO), STATE_MACHINE_OK);

    ASSERT_EQ(nest_rc, STATE_MACHINE_ERR_BUSY);
    ASSERT_EQ(state_machine_current(&nest_sm), N_B);
    ASSERT_EQ(nest_exit[N_A], 1);
    ASSERT_EQ(nest_enter[N_B], 1);
    ASSERT_EQ(nest_enter[N_C], 0);
    ASSERT_EQ(state_machine_transition_count(&nest_sm), 1);

    ASSERT_EQ(state_machine_process(&nest_sm, NE_NEXT), STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_current(&nest_sm), N_C);
}

TEST(test_process_from_on_enter_is_refused)
{
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_plain), nest_defs_enter_posts, N_A),
              STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_process(&nest_sm, NE_GO), STATE_MACHINE_OK);

    ASSERT_EQ(nest_rc, STATE_MACHINE_ERR_BUSY);
    ASSERT_EQ(state_machine_current(&nest_sm), N_B);
    ASSERT_EQ(nest_exit[N_B], 0);
    ASSERT_EQ(nest_enter[N_C], 0);
    ASSERT_EQ(state_machine_transition_count(&nest_sm), 1);

    ASSERT_EQ(state_machine_process(&nest_sm, NE_NEXT), STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_current(&nest_sm), N_C);
}

TEST(test_process_from_on_exit_is_refused)
{
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_plain), nest_defs_exit_posts, N_A),
              STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_process(&nest_sm, NE_GO), STATE_MACHINE_OK);

    ASSERT_EQ(nest_rc, STATE_MACHINE_ERR_BUSY);
    ASSERT_EQ(state_machine_current(&nest_sm), N_B);
    ASSERT_EQ(nest_exit[N_A], 1);
    ASSERT_EQ(nest_enter[N_C], 0);
    ASSERT_EQ(state_machine_transition_count(&nest_sm), 1);
}

TEST(test_process_from_guard_is_refused)
{
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_guard_posts), nest_defs, N_A),
              STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_process(&nest_sm, NE_GO), STATE_MACHINE_OK);

    ASSERT_EQ(nest_rc, STATE_MACHINE_ERR_BUSY);
    ASSERT_EQ(state_machine_current(&nest_sm), N_B);
    ASSERT_EQ(nest_exit[N_A], 1);
    ASSERT_EQ(nest_enter[N_C], 0);
    ASSERT_EQ(state_machine_transition_count(&nest_sm), 1);
}

TEST(test_reset_from_action_is_refused)
{
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_action_resets), nest_defs, N_A),
              STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_process(&nest_sm, NE_GO), STATE_MACHINE_OK);

    ASSERT_EQ(nest_rc, STATE_MACHINE_ERR_BUSY);
    ASSERT_EQ(state_machine_current(&nest_sm), N_B);
    ASSERT_EQ(nest_enter[N_C], 0);
    ASSERT_EQ(state_machine_transition_count(&nest_sm), 1);
}

TEST(test_no_transition_leaves_the_machine_usable)
{
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_guard_blocks), nest_defs, N_A),
              STATE_MACHINE_OK);

    /* A guard said no, and then an event with no row at all */
    ASSERT_EQ(state_machine_process(&nest_sm, NE_GO),
              STATE_MACHINE_ERR_NO_TRANSITION);
    ASSERT_EQ(state_machine_process(&nest_sm, 99),
              STATE_MACHINE_ERR_NO_TRANSITION);

    ASSERT_EQ(state_machine_process(&nest_sm, NE_NEXT), STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_current(&nest_sm), N_C);
}

TEST(test_reset_leaves_the_machine_usable)
{
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_plain), nest_defs, N_A),
              STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_reset(&nest_sm, N_B), STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_process(&nest_sm, NE_NEXT), STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_current(&nest_sm), N_C);
}

TEST(test_process_from_on_enter_during_init_still_runs)
{
    /* No transition is under way, so nothing can overwrite the result. */
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_plain), nest_defs_enter_posts, N_B),
              STATE_MACHINE_OK);

    ASSERT_EQ(nest_rc, STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_current(&nest_sm), N_C);
    ASSERT_EQ(nest_exit[N_B], 1);
    ASSERT_EQ(nest_enter[N_C], 1);
}

TEST(test_process_from_on_enter_during_reset_still_runs)
{
    ASSERT_EQ(nest_setup(NEST_TABLE(nest_plain), nest_defs_enter_posts, N_C),
              STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_reset(&nest_sm, N_B), STATE_MACHINE_OK);

    ASSERT_EQ(nest_rc, STATE_MACHINE_OK);
    ASSERT_EQ(state_machine_current(&nest_sm), N_C);
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
    RUN(test_process_from_action_is_refused);
    RUN(test_process_from_on_enter_is_refused);
    RUN(test_process_from_on_exit_is_refused);
    RUN(test_process_from_guard_is_refused);
    RUN(test_reset_from_action_is_refused);
    RUN(test_no_transition_leaves_the_machine_usable);
    RUN(test_reset_leaves_the_machine_usable);
    RUN(test_process_from_on_enter_during_init_still_runs);
    RUN(test_process_from_on_enter_during_reset_still_runs);
TEST_MAIN_END()

