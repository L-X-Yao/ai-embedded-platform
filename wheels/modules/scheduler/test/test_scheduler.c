// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "scheduler.h"

#include <string.h>

/* ----- test helpers ----- */

static int g_call_count = 0;
static void *g_last_ctx = NULL;

static void task_counter(void *ctx)
{
    (void)ctx;
    g_call_count++;
}

static void task_ctx_recorder(void *ctx)
{
    g_last_ctx = ctx;
}

static int g_call_a = 0;
static int g_call_b = 0;

static void task_a(void *ctx) { (void)ctx; g_call_a++; }
static void task_b(void *ctx) { (void)ctx; g_call_b++; }

static void reset_globals(void)
{
    g_call_count = 0;
    g_last_ctx = NULL;
    g_call_a = 0;
    g_call_b = 0;
}

/* ===================== tests ===================== */

TEST(test_init)
{
    scheduler_t s;
    ASSERT_EQ(scheduler_init(&s), SCHED_OK);
    ASSERT_EQ(scheduler_count(&s), 0);
}

TEST(test_init_null)
{
    ASSERT_EQ(scheduler_init(NULL), SCHED_ERR_PARAM);
}

TEST(test_add_task)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    ASSERT_EQ(scheduler_add(&s, "t1", task_counter, NULL, 10), SCHED_OK);
    ASSERT_EQ(scheduler_count(&s), 1);
}

TEST(test_add_null_params)
{
    scheduler_t s;
    scheduler_init(&s);

    ASSERT_EQ(scheduler_add(NULL, "t1", task_counter, NULL, 10), SCHED_ERR_PARAM);
    ASSERT_EQ(scheduler_add(&s, NULL, task_counter, NULL, 10), SCHED_ERR_PARAM);
    ASSERT_EQ(scheduler_add(&s, "t1", NULL, NULL, 10), SCHED_ERR_PARAM);
    ASSERT_EQ(scheduler_count(&s), 0);
}

TEST(test_run_at_interval)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "t1", task_counter, NULL, 5);

    /* advance 5 ticks, then run -- task should fire */
    for (int i = 0; i < 5; i++) scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_EQ(g_call_count, 1);
}

TEST(test_run_before_interval)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "t1", task_counter, NULL, 5);

    /* advance only 3 ticks -- should NOT fire */
    for (int i = 0; i < 3; i++) scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_EQ(g_call_count, 0);
}

TEST(test_multiple_tasks_different_intervals)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "fast", task_a, NULL, 2);
    scheduler_add(&s, "slow", task_b, NULL, 5);

    /* advance 5 ticks, run after each tick */
    for (int i = 0; i < 5; i++) {
        scheduler_tick(&s);
        scheduler_run(&s);
    }

    /* fast: fires at tick 2, 4 => 2 times */
    ASSERT_EQ(g_call_a, 2);
    /* slow: fires at tick 5 => 1 time */
    ASSERT_EQ(g_call_b, 1);
}

TEST(test_enable_disable)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "t1", task_counter, NULL, 1);

    /* disable, advance, run -- should not fire */
    ASSERT_EQ(scheduler_disable(&s, "t1"), SCHED_OK);
    scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_EQ(g_call_count, 0);

    /* re-enable, advance, run -- should fire */
    ASSERT_EQ(scheduler_enable(&s, "t1"), SCHED_OK);
    scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_EQ(g_call_count, 1);
}

TEST(test_enable_disable_not_found)
{
    scheduler_t s;
    scheduler_init(&s);

    ASSERT_EQ(scheduler_enable(&s, "nope"), SCHED_ERR_PARAM);
    ASSERT_EQ(scheduler_disable(&s, "nope"), SCHED_ERR_PARAM);
    ASSERT_EQ(scheduler_enable(NULL, "x"), SCHED_ERR_PARAM);
    ASSERT_EQ(scheduler_disable(NULL, "x"), SCHED_ERR_PARAM);
}

TEST(test_remove)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "t1", task_counter, NULL, 1);
    ASSERT_EQ(scheduler_count(&s), 1);

    ASSERT_EQ(scheduler_remove(&s, "t1"), SCHED_OK);
    ASSERT_EQ(scheduler_count(&s), 0);

    /* run after remove -- should not fire */
    scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_EQ(g_call_count, 0);
}

TEST(test_remove_not_found)
{
    scheduler_t s;
    scheduler_init(&s);

    ASSERT_EQ(scheduler_remove(&s, "nope"), SCHED_ERR_PARAM);
    ASSERT_EQ(scheduler_remove(NULL, "x"), SCHED_ERR_PARAM);
}

TEST(test_max_tasks)
{
    scheduler_t s;
    scheduler_init(&s);

    char names[SCHED_MAX_TASKS][8];
    for (int i = 0; i < SCHED_MAX_TASKS; i++) {
        /* build unique name */
        names[i][0] = 't';
        names[i][1] = (char)('0' + (i / 10));
        names[i][2] = (char)('0' + (i % 10));
        names[i][3] = '\0';
        ASSERT_EQ(scheduler_add(&s, names[i], task_counter, NULL, 1), SCHED_OK);
    }
    ASSERT_EQ(scheduler_count(&s), SCHED_MAX_TASKS);

    /* one more should fail */
    ASSERT_EQ(scheduler_add(&s, "extra", task_counter, NULL, 1), SCHED_ERR_FULL);
}

TEST(test_run_count)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "t1", task_counter, NULL, 2);

    /* 6 ticks, run each -- should fire at 2,4,6 = 3 times */
    for (int i = 0; i < 6; i++) {
        scheduler_tick(&s);
        scheduler_run(&s);
    }
    ASSERT_EQ(scheduler_get_run_count(&s, "t1"), 3);
}

TEST(test_run_count_not_found)
{
    scheduler_t s;
    scheduler_init(&s);
    ASSERT_EQ(scheduler_get_run_count(&s, "nope"), 0);
    ASSERT_EQ(scheduler_get_run_count(NULL, "x"), 0);
}

TEST(test_task_context)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    int marker = 42;
    scheduler_add(&s, "ctx_task", task_ctx_recorder, &marker, 1);

    scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_TRUE(g_last_ctx == &marker);
}

TEST(test_zero_interval)
{
    /* interval 0 means "every tick" */
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "every", task_counter, NULL, 0);

    /* should fire on every run (elapsed always >= 0) */
    scheduler_run(&s);
    ASSERT_EQ(g_call_count, 1);

    scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_EQ(g_call_count, 2);

    scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_EQ(g_call_count, 3);
}

TEST(test_overrun_detection)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "t1", task_counter, NULL, 5);

    /* advance 11 ticks without running -- missed > 1 interval */
    for (int i = 0; i < 11; i++) scheduler_tick(&s);
    scheduler_run(&s);

    ASSERT_EQ(g_call_count, 1);
    /* overrun_count should be 1 (elapsed=11 > interval=5) */
    ASSERT_EQ(s.tasks[0].overrun_count, 1);
}

TEST(test_find_by_name)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "alpha", task_a, NULL, 1);
    scheduler_add(&s, "beta", task_b, NULL, 1);

    /* verify enable/disable targets the right task */
    scheduler_disable(&s, "alpha");
    scheduler_tick(&s);
    scheduler_run(&s);

    ASSERT_EQ(g_call_a, 0);
    ASSERT_EQ(g_call_b, 1);
}

TEST(test_tick_null_safety)
{
    /* should not crash */
    scheduler_tick(NULL);
    scheduler_run(NULL);
}

TEST(test_remove_middle_preserves_order)
{
    scheduler_t s;
    scheduler_init(&s);
    reset_globals();

    scheduler_add(&s, "first", task_a, NULL, 1);
    scheduler_add(&s, "middle", task_counter, NULL, 1);
    scheduler_add(&s, "last", task_b, NULL, 1);

    scheduler_remove(&s, "middle");
    ASSERT_EQ(scheduler_count(&s), 2);

    /* verify remaining tasks run correctly */
    scheduler_tick(&s);
    scheduler_run(&s);
    ASSERT_EQ(g_call_a, 1);
    ASSERT_EQ(g_call_b, 1);
    ASSERT_EQ(g_call_count, 0); /* removed task should not run */
}

TEST_MAIN("scheduler")
    RUN(test_init);
    RUN(test_init_null);
    RUN(test_add_task);
    RUN(test_add_null_params);
    RUN(test_run_at_interval);
    RUN(test_run_before_interval);
    RUN(test_multiple_tasks_different_intervals);
    RUN(test_enable_disable);
    RUN(test_enable_disable_not_found);
    RUN(test_remove);
    RUN(test_remove_not_found);
    RUN(test_max_tasks);
    RUN(test_run_count);
    RUN(test_run_count_not_found);
    RUN(test_task_context);
    RUN(test_zero_interval);
    RUN(test_overrun_detection);
    RUN(test_find_by_name);
    RUN(test_tick_null_safety);
    RUN(test_remove_middle_preserves_order);
TEST_MAIN_END()
