// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "task_watchdog.h"

/* ---- callback spy ------------------------------------------------------ */

static int         cb_count;
static const char *cb_last_name;
static uint32_t    cb_last_feed;
static void       *cb_last_ctx;

static void spy_cb(void *ctx, const char *name, uint32_t last_feed)
{
    cb_count++;
    cb_last_name = name;
    cb_last_feed = last_feed;
    cb_last_ctx  = ctx;
}

static void reset_spy(void)
{
    cb_count     = 0;
    cb_last_name = NULL;
    cb_last_feed = 0;
    cb_last_ctx  = NULL;
}

/* ---- helpers ----------------------------------------------------------- */

static void advance_ticks(task_watchdog_t *wd, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
    {
        task_watchdog_tick(wd);
    }
}

/* ---- tests ------------------------------------------------------------- */

TEST(test_init)
{
    task_watchdog_t wd;
    ASSERT_EQ(task_watchdog_init(&wd), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_count(&wd), 0);
    ASSERT_EQ(task_watchdog_expired_count(&wd), 0);
}

TEST(test_init_null)
{
    ASSERT_EQ(task_watchdog_init(NULL), TASK_WD_ERR_PARAM);
}

TEST(test_register_feed_check_ok)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    reset_spy();
    task_watchdog_set_callback(&wd, spy_cb, NULL);

    ASSERT_EQ(task_watchdog_register(&wd, "sensor", 10), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_count(&wd), 1);

    /* feed within timeout — no expiry */
    advance_ticks(&wd, 5);
    task_watchdog_feed(&wd, "sensor");
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 0);
    ASSERT_EQ(task_watchdog_status(&wd, "sensor"), TASK_WD_STATUS_OK);
}

TEST(test_expired_after_timeout)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    reset_spy();
    task_watchdog_set_callback(&wd, spy_cb, NULL);

    task_watchdog_register(&wd, "motor", 5);

    /* advance past timeout without feeding */
    advance_ticks(&wd, 5);
    task_watchdog_check(&wd);

    ASSERT_EQ(cb_count, 1);
    ASSERT_TRUE(strcmp(cb_last_name, "motor") == 0);
    ASSERT_EQ(task_watchdog_status(&wd, "motor"), TASK_WD_STATUS_EXPIRED);
}

TEST(test_callback_receives_context)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    reset_spy();

    int user_data = 42;
    task_watchdog_set_callback(&wd, spy_cb, &user_data);
    task_watchdog_register(&wd, "comm", 3);

    advance_ticks(&wd, 3);
    task_watchdog_check(&wd);

    ASSERT_EQ(cb_count, 1);
    ASSERT_TRUE(cb_last_ctx == &user_data);
}

TEST(test_multiple_tasks_mixed)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    reset_spy();
    task_watchdog_set_callback(&wd, spy_cb, NULL);

    task_watchdog_register(&wd, "fast", 10);
    task_watchdog_register(&wd, "slow", 3);

    /* advance 3 ticks — slow expires, fast OK */
    advance_ticks(&wd, 3);
    task_watchdog_check(&wd);

    ASSERT_EQ(cb_count, 1);
    ASSERT_TRUE(strcmp(cb_last_name, "slow") == 0);
    ASSERT_EQ(task_watchdog_status(&wd, "fast"), TASK_WD_STATUS_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "slow"), TASK_WD_STATUS_EXPIRED);
    ASSERT_EQ(task_watchdog_expired_count(&wd), 1);
}

TEST(test_feed_resets_timer)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    reset_spy();
    task_watchdog_set_callback(&wd, spy_cb, NULL);

    task_watchdog_register(&wd, "task1", 5);

    /* advance 4, feed, advance 4 — should not expire (4+4=8, but fed at 4) */
    advance_ticks(&wd, 4);
    task_watchdog_feed(&wd, "task1");
    advance_ticks(&wd, 4);
    task_watchdog_check(&wd);

    ASSERT_EQ(cb_count, 0);
    ASSERT_EQ(task_watchdog_status(&wd, "task1"), TASK_WD_STATUS_OK);
}

TEST(test_disable_enable)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    reset_spy();
    task_watchdog_set_callback(&wd, spy_cb, NULL);

    task_watchdog_register(&wd, "ctrl", 5);

    /* disable — should not fire even after timeout */
    task_watchdog_disable(&wd, "ctrl");
    ASSERT_EQ(task_watchdog_status(&wd, "ctrl"), TASK_WD_STATUS_DISABLED);

    advance_ticks(&wd, 10);
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 0);

    /* re-enable — timer resets, should not fire immediately */
    task_watchdog_enable(&wd, "ctrl");
    ASSERT_EQ(task_watchdog_status(&wd, "ctrl"), TASK_WD_STATUS_OK);

    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 0);

    /* now let it expire */
    advance_ticks(&wd, 5);
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 1);
}

TEST(test_max_tasks)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);

    char names[TASK_WD_MAX_TASKS + 1][8];
    for (int i = 0; i < TASK_WD_MAX_TASKS; i++)
    {
        /* build unique name per slot */
        names[i][0] = 't';
        names[i][1] = (char)('0' + (i / 10));
        names[i][2] = (char)('0' + (i % 10));
        names[i][3] = '\0';
        ASSERT_EQ(task_watchdog_register(&wd, names[i], 100), TASK_WD_OK);
    }
    ASSERT_EQ(task_watchdog_count(&wd), TASK_WD_MAX_TASKS);

    /* one more should fail */
    ASSERT_EQ(task_watchdog_register(&wd, "extra", 100), TASK_WD_ERR_FULL);
}

TEST(test_feed_unknown_task)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);

    ASSERT_EQ(task_watchdog_feed(&wd, "ghost"), TASK_WD_ERR_NOT_FOUND);
}

TEST(test_status_query)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);

    task_watchdog_register(&wd, "a", 5);

    /* initially OK */
    ASSERT_EQ(task_watchdog_status(&wd, "a"), TASK_WD_STATUS_OK);

    /* unknown task returns DISABLED */
    ASSERT_EQ(task_watchdog_status(&wd, "nope"), TASK_WD_STATUS_DISABLED);
}

TEST(test_expired_count)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    reset_spy();

    task_watchdog_register(&wd, "x", 3);
    task_watchdog_register(&wd, "y", 3);

    ASSERT_EQ(task_watchdog_expired_count(&wd), 0);

    advance_ticks(&wd, 3);
    task_watchdog_check(&wd);

    ASSERT_EQ(task_watchdog_expired_count(&wd), 2);
}

TEST(test_refeed_after_expiry)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    reset_spy();
    task_watchdog_set_callback(&wd, spy_cb, NULL);

    task_watchdog_register(&wd, "rec", 5);

    /* let it expire */
    advance_ticks(&wd, 5);
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(task_watchdog_status(&wd, "rec"), TASK_WD_STATUS_EXPIRED);

    /* re-feed clears expired status */
    task_watchdog_feed(&wd, "rec");
    ASSERT_EQ(task_watchdog_status(&wd, "rec"), TASK_WD_STATUS_OK);

    /* check again — should not fire (timer was reset by feed) */
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 1);  /* still 1, no new callback */
}

TEST(test_tick_increments)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);

    advance_ticks(&wd, 7);
    /* tick_count is internal — verify via expiry timing */
    task_watchdog_register(&wd, "t", 3);  /* last_feed set to current tick=7 */

    advance_ticks(&wd, 2);
    reset_spy();
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 0);  /* only 2 ticks since register, timeout=3 */

    advance_ticks(&wd, 1);
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 1);  /* 3 ticks since register, exactly at timeout */
}

TEST(test_null_params)
{
    /* tick / check / set_callback tolerate NULL gracefully */
    task_watchdog_tick(NULL);
    task_watchdog_check(NULL);
    task_watchdog_set_callback(NULL, spy_cb, NULL);

    ASSERT_EQ(task_watchdog_count(NULL), 0);
    ASSERT_EQ(task_watchdog_expired_count(NULL), 0);
    ASSERT_EQ(task_watchdog_feed(NULL, "x"), TASK_WD_ERR_PARAM);
    ASSERT_EQ(task_watchdog_register(NULL, "x", 5), TASK_WD_ERR_PARAM);
    ASSERT_EQ(task_watchdog_enable(NULL, "x"), TASK_WD_ERR_PARAM);
    ASSERT_EQ(task_watchdog_disable(NULL, "x"), TASK_WD_ERR_PARAM);
    ASSERT_EQ(task_watchdog_status(NULL, "x"), TASK_WD_STATUS_DISABLED);
}

TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
    task_watchdog_t wd;
    memset(&wd, 0, sizeof(wd));
    ASSERT_EQ(task_watchdog_count(&wd), 0);
    ASSERT_EQ(task_watchdog_expired_count(&wd), 0);
    task_watchdog_check(&wd);
    task_watchdog_tick(&wd);
}

TEST(test_no_callback_no_crash)
{
    /* Expiry without callback set must not crash */
    task_watchdog_t wd;
    task_watchdog_init(&wd);

    task_watchdog_register(&wd, "quiet", 2);
    advance_ticks(&wd, 5);
    task_watchdog_check(&wd);

    ASSERT_EQ(task_watchdog_status(&wd, "quiet"), TASK_WD_STATUS_EXPIRED);
}

TEST(test_expired_callback_fires_once)
{
    /* Once expired, repeated check() must not re-fire callback */
    task_watchdog_t wd;
    task_watchdog_init(&wd);
    reset_spy();
    task_watchdog_set_callback(&wd, spy_cb, NULL);

    task_watchdog_register(&wd, "once", 3);
    advance_ticks(&wd, 3);

    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 1);

    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 1);  /* still 1 */

    advance_ticks(&wd, 10);
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 1);  /* still 1 */
}

TEST(test_register_bad_params)
{
    task_watchdog_t wd;
    task_watchdog_init(&wd);

    ASSERT_EQ(task_watchdog_register(&wd, NULL, 5), TASK_WD_ERR_PARAM);
    ASSERT_EQ(task_watchdog_register(&wd, "x", 0), TASK_WD_ERR_PARAM);
}

/* A restarted task registers again.  A second entry would be one that feed
 * never reaches, and would expire for a task that is being fed. */
TEST(test_register_again_does_not_expire_a_fed_task)
{
    task_watchdog_t wd;

    reset_spy();
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    ASSERT_EQ(task_watchdog_register(&wd, "t", 5), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_register(&wd, "t", 5), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_count(&wd), 1);

    for (int i = 0; i < 20; i++)
    {
        task_watchdog_tick(&wd);
        ASSERT_EQ(task_watchdog_feed(&wd, "t"), TASK_WD_OK);
        task_watchdog_check(&wd);
    }
    ASSERT_EQ(cb_count, 0);
}

/* It takes over the entry whatever state that was in, with its new timeout. */
TEST(test_register_again_takes_over_the_entry)
{
    task_watchdog_t wd;

    reset_spy();
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    task_watchdog_register(&wd, "t", 5);
    advance_ticks(&wd, 5);
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_EXPIRED);

    ASSERT_EQ(task_watchdog_register(&wd, "t", 10), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_OK);
    advance_ticks(&wd, 9);
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_OK);
    advance_ticks(&wd, 1);
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_EXPIRED);

    task_watchdog_disable(&wd, "t");
    ASSERT_EQ(task_watchdog_register(&wd, "t", 10), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_OK);
    advance_ticks(&wd, 10);
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_EXPIRED);
}

/* Restarts must not use up the table, and a full table still takes them. */
TEST(test_register_again_needs_no_free_slot)
{
    task_watchdog_t wd;
    char names[TASK_WD_MAX_TASKS][8];

    task_watchdog_init(&wd);
    for (int i = 0; i < TASK_WD_MAX_TASKS; i++)
    {
        names[i][0] = 't';
        names[i][1] = (char)('0' + (i / 10));
        names[i][2] = (char)('0' + (i % 10));
        names[i][3] = '\0';
        ASSERT_EQ(task_watchdog_register(&wd, names[i], 100), TASK_WD_OK);
    }

    for (int i = 0; i < 3 * TASK_WD_MAX_TASKS; i++)
    {
        ASSERT_EQ(task_watchdog_register(&wd, names[0], 100), TASK_WD_OK);
    }
    ASSERT_EQ(task_watchdog_count(&wd), TASK_WD_MAX_TASKS);
    ASSERT_EQ(task_watchdog_register(&wd, "extra", 100), TASK_WD_ERR_FULL);
}

/* Nothing watches a disabled task, so it must not report a healthy status. */
TEST(test_feed_leaves_a_disabled_task_disabled)
{
    task_watchdog_t wd;

    reset_spy();
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    task_watchdog_register(&wd, "t", 5);
    task_watchdog_disable(&wd, "t");

    ASSERT_EQ(task_watchdog_feed(&wd, "t"), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_DISABLED);

    advance_ticks(&wd, 100);
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_DISABLED);
    ASSERT_EQ(cb_count, 0);

    /* enabling starts it fresh, and from then on it can expire */
    ASSERT_EQ(task_watchdog_enable(&wd, "t"), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_OK);
    advance_ticks(&wd, 5);
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "t"), TASK_WD_STATUS_EXPIRED);
    ASSERT_EQ(cb_count, 1);
}

/* A feed for one task must not keep another, hung one alive. */
TEST(test_feed_reaches_only_the_named_task)
{
    task_watchdog_t wd;

    reset_spy();
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    task_watchdog_register(&wd, "first", 5);
    task_watchdog_register(&wd, "second", 5);

    advance_ticks(&wd, 4);
    ASSERT_EQ(task_watchdog_feed(&wd, "second"), TASK_WD_OK);
    advance_ticks(&wd, 1);
    task_watchdog_check(&wd);

    ASSERT_EQ(task_watchdog_status(&wd, "first"), TASK_WD_STATUS_EXPIRED);
    ASSERT_EQ(task_watchdog_status(&wd, "second"), TASK_WD_STATUS_OK);
    ASSERT_EQ(cb_count, 1);
    ASSERT_STR_EQ(cb_last_name, "first");
    ASSERT_EQ(task_watchdog_expired_count(&wd), 1);
}

TEST(test_disable_reaches_only_the_named_task)
{
    task_watchdog_t wd;

    reset_spy();
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    task_watchdog_register(&wd, "first", 5);
    task_watchdog_register(&wd, "second", 5);

    ASSERT_EQ(task_watchdog_disable(&wd, "second"), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "first"), TASK_WD_STATUS_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "second"), TASK_WD_STATUS_DISABLED);

    advance_ticks(&wd, 5);
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "first"), TASK_WD_STATUS_EXPIRED);
    ASSERT_EQ(task_watchdog_status(&wd, "second"), TASK_WD_STATUS_DISABLED);
    ASSERT_EQ(cb_count, 1);
    ASSERT_STR_EQ(cb_last_name, "first");
}

/* The header's rule, pinned: the name is kept by pointer. */
TEST(test_name_is_kept_by_pointer)
{
    task_watchdog_t wd;
    char name[] = "t0";

    reset_spy();
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    ASSERT_EQ(task_watchdog_register(&wd, name, 5), TASK_WD_OK);

    /* A second task named in the same buffer reads as a restart of the first */
    name[1] = '1';
    ASSERT_EQ(task_watchdog_register(&wd, name, 5), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_count(&wd), 1);

    /* The expiry callback gets the pointer it was given */
    advance_ticks(&wd, 6);
    task_watchdog_check(&wd);
    ASSERT_EQ(cb_count, 1);
    ASSERT_TRUE(cb_last_name == name);
}

TEST(test_enable_reaches_only_the_named_task)
{
    task_watchdog_t wd;

    reset_spy();
    task_watchdog_init(&wd);
    task_watchdog_set_callback(&wd, spy_cb, NULL);
    task_watchdog_register(&wd, "first", 5);
    task_watchdog_register(&wd, "second", 5);
    task_watchdog_disable(&wd, "first");
    task_watchdog_disable(&wd, "second");

    ASSERT_EQ(task_watchdog_enable(&wd, "second"), TASK_WD_OK);
    ASSERT_EQ(task_watchdog_status(&wd, "first"), TASK_WD_STATUS_DISABLED);
    ASSERT_EQ(task_watchdog_status(&wd, "second"), TASK_WD_STATUS_OK);

    advance_ticks(&wd, 5);
    task_watchdog_check(&wd);
    ASSERT_EQ(task_watchdog_status(&wd, "first"), TASK_WD_STATUS_DISABLED);
    ASSERT_EQ(task_watchdog_status(&wd, "second"), TASK_WD_STATUS_EXPIRED);
    ASSERT_STR_EQ(cb_last_name, "second");
}

TEST_MAIN("task_watchdog")
    RUN(test_init);
    RUN(test_init_null);
    RUN(test_register_feed_check_ok);
    RUN(test_expired_after_timeout);
    RUN(test_callback_receives_context);
    RUN(test_multiple_tasks_mixed);
    RUN(test_feed_resets_timer);
    RUN(test_disable_enable);
    RUN(test_max_tasks);
    RUN(test_feed_unknown_task);
    RUN(test_status_query);
    RUN(test_expired_count);
    RUN(test_refeed_after_expiry);
    RUN(test_tick_increments);
    RUN(test_null_params);
    RUN(test_zero_init_safety);
    RUN(test_no_callback_no_crash);
    RUN(test_expired_callback_fires_once);
    RUN(test_register_bad_params);
    RUN(test_register_again_does_not_expire_a_fed_task);
    RUN(test_register_again_takes_over_the_entry);
    RUN(test_register_again_needs_no_free_slot);
    RUN(test_feed_leaves_a_disabled_task_disabled);
    RUN(test_feed_reaches_only_the_named_task);
    RUN(test_disable_reaches_only_the_named_task);
    RUN(test_enable_reaches_only_the_named_task);
    RUN(test_name_is_kept_by_pointer);
TEST_MAIN_END()
