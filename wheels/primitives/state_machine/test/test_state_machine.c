#include <stdio.h>
#include <string.h>
#include "../state_machine.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)

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

static void test_init(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();
    ASSERT_EQ(state_machine_current(&sm), S_RED, "starts in RED");
    ASSERT_EQ(state_machine_transition_count(&sm), 0, "zero transitions");
    ASSERT_EQ(enter_count[S_RED], 1, "on_enter called for initial state");
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    sm_cfg_t cfg = {0};
    ASSERT_EQ(state_machine_init(NULL, &cfg, 0), STATE_MACHINE_ERR_PARAM, "init null ctx");
    ASSERT_EQ(state_machine_init(&sm, NULL, 0), STATE_MACHINE_ERR_PARAM, "init null cfg");
    ASSERT_EQ(state_machine_process(NULL, 0), STATE_MACHINE_ERR_PARAM, "process null");
    ASSERT_EQ(state_machine_current(NULL), 0, "current null");
    ASSERT_EQ(state_machine_transition_count(NULL), 0, "count null");
    ASSERT_EQ(state_machine_reset(NULL, 0), STATE_MACHINE_ERR_PARAM, "reset null");
}

static void test_basic_transition(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();
    state_machine_err_t err = state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(err, STATE_MACHINE_OK, "transition ok");
    ASSERT_EQ(state_machine_current(&sm), S_GREEN, "RED->GREEN on TIMER");
    ASSERT_EQ(state_machine_transition_count(&sm), 1, "count=1");
}

static void test_full_cycle(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_GREEN, "step 1");
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_YELLOW, "step 2");
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_RED, "step 3 back to RED");
    ASSERT_EQ(state_machine_transition_count(&sm), 3, "3 transitions");
}

static void test_entry_exit_actions(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();

    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(exit_count[S_RED], 1, "exit RED called");
    ASSERT_EQ(enter_count[S_GREEN], 1, "enter GREEN called");
    ASSERT_EQ(action_count, 1, "action called");
}

static void test_emergency(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_GREEN, "at GREEN");

    state_machine_process(&sm, E_EMERGENCY);
    ASSERT_EQ(state_machine_current(&sm), S_RED, "emergency -> RED");
}

static void test_no_transition(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();
    state_machine_err_t err = state_machine_process(&sm, E_EMERGENCY);
    ASSERT_EQ(err, STATE_MACHINE_ERR_NO_TRANSITION, "no emergency from RED");
    ASSERT_EQ(state_machine_current(&sm), S_RED, "still RED");
    ASSERT_EQ(state_machine_transition_count(&sm), 0, "no transition counted");
}

static void test_unknown_event(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();
    state_machine_err_t err = state_machine_process(&sm, 99);
    ASSERT_EQ(err, STATE_MACHINE_ERR_NO_TRANSITION, "unknown event");
    ASSERT_EQ(state_machine_current(&sm), S_RED, "state unchanged");
}

static void test_guard_pass(void)
{
    printf("%s\n", __func__);
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
    ASSERT_EQ(err, STATE_MACHINE_OK, "guard pass");
    ASSERT_EQ(state_machine_current(&sm), S_GREEN, "transitioned");
}

static void test_guard_block(void)
{
    printf("%s\n", __func__);
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
    ASSERT_EQ(err, STATE_MACHINE_ERR_NO_TRANSITION, "guard blocked");
    ASSERT_EQ(state_machine_current(&sm), S_RED, "still RED");
}

static void test_self_transition(void)
{
    printf("%s\n", __func__);
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
    ASSERT_EQ(state_machine_current(&sm), S_RED, "self-transition stays");
    ASSERT_EQ(action_count, 1, "action called on self-transition");
    ASSERT_EQ(enter_count[S_RED], 0, "no re-enter on self-transition");
    ASSERT_EQ(exit_count[S_RED], 0, "no exit on self-transition");
}

static void test_reset(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();
    state_machine_process(&sm, E_TIMER);
    state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(state_machine_current(&sm), S_YELLOW, "at YELLOW");

    state_machine_reset(&sm, S_RED);
    ASSERT_EQ(state_machine_current(&sm), S_RED, "reset to RED");
    ASSERT_EQ(state_machine_transition_count(&sm), 0, "count cleared");
}

static void test_no_state_defs(void)
{
    printf("%s\n", __func__);
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
    ASSERT_EQ(state_machine_current(&sm), S_GREEN, "works without state defs");
    ASSERT_EQ(enter_count[S_GREEN], 0, "no enter callback");
}

static void test_no_transitions(void)
{
    printf("%s\n", __func__);
    sm_cfg_t cfg = {
        .transitions = NULL,
        .num_transitions = 0,
        .state_defs = NULL,
        .num_state_defs = 0,
        .user_ctx = NULL,
    };
    state_machine_init(&sm, &cfg, S_RED);
    state_machine_err_t err = state_machine_process(&sm, E_TIMER);
    ASSERT_EQ(err, STATE_MACHINE_ERR_NO_TRANSITION, "no transitions defined");
}

static void test_multiple_cycles(void)
{
    printf("%s\n", __func__);
    setup_traffic_light();
    for (int i = 0; i < 9; i++)
    {
        state_machine_process(&sm, E_TIMER);
    }
    ASSERT_EQ(state_machine_current(&sm), S_RED, "3 full cycles back to RED");
    ASSERT_EQ(state_machine_transition_count(&sm), 9, "9 transitions total");
}

int main(void)
{
    printf("test_state_machine\n");
    test_init();
    test_null_safety();
    test_basic_transition();
    test_full_cycle();
    test_entry_exit_actions();
    test_emergency();
    test_no_transition();
    test_unknown_event();
    test_guard_pass();
    test_guard_block();
    test_self_transition();
    test_reset();
    test_no_state_defs();
    test_no_transitions();
    test_multiple_cycles();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
