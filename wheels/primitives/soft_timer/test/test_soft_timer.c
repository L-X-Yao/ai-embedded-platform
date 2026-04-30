#include "test_harness.h"
#include "soft_timer.h"
static int cb_count;
static void *cb_last_ctx;

static void test_callback(void *ctx)
{
    cb_count++;
    cb_last_ctx = ctx;
}

static int cb2_count;
static void test_callback2(void *ctx)
{
    (void)ctx;
    cb2_count++;
}

static soft_timer_mgr_t mgr;

static void setup(void)
{
    soft_timer_init(&mgr);
    cb_count = 0;
    cb2_count = 0;
    cb_last_ctx = NULL;
}

TEST(test_init)
{
    setup();
    ASSERT_EQ(mgr.count, 0);
    ASSERT_EQ(mgr.tick_count, 0);
}

TEST(test_null_safety)
{
    ASSERT_EQ(soft_timer_init(NULL), SOFT_TIMER_ERR_PARAM);
    ASSERT_EQ(soft_timer_create(NULL, 10, SOFT_TIMER_ONE_SHOT, test_callback, NULL),
              SOFT_TIMER_ERR_PARAM);
    setup();
    ASSERT_EQ(soft_timer_create(&mgr, 0, SOFT_TIMER_ONE_SHOT, test_callback, NULL),
              SOFT_TIMER_ERR_PARAM);
    ASSERT_EQ(soft_timer_create(&mgr, 10, SOFT_TIMER_ONE_SHOT, NULL, NULL),
              SOFT_TIMER_ERR_PARAM);
    ASSERT_EQ(soft_timer_start(NULL, 0), SOFT_TIMER_ERR_PARAM);
    ASSERT_EQ(soft_timer_stop(NULL, 0), SOFT_TIMER_ERR_PARAM);
    ASSERT_EQ(soft_timer_reset(NULL, 0), SOFT_TIMER_ERR_PARAM);
    ASSERT_TRUE(!soft_timer_is_active(NULL, 0));
}

TEST(test_create_and_start)
{
    setup();
    int id = soft_timer_create(&mgr, 5, SOFT_TIMER_ONE_SHOT, test_callback, NULL);
    ASSERT_TRUE(id >= 0);
    ASSERT_EQ(mgr.count, 1);
    ASSERT_TRUE(!soft_timer_is_active(&mgr, id));

    soft_timer_start(&mgr, id);
    ASSERT_TRUE(soft_timer_is_active(&mgr, id));
}

TEST(test_one_shot_fires)
{
    setup();
    int id = soft_timer_create(&mgr, 3, SOFT_TIMER_ONE_SHOT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);
    ASSERT_TRUE(!soft_timer_is_active(&mgr, id));

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);
}

TEST(test_repeat_fires)
{
    setup();
    int id = soft_timer_create(&mgr, 2, SOFT_TIMER_REPEAT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 2);

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 3);
    ASSERT_TRUE(soft_timer_is_active(&mgr, id));
}

TEST(test_stop)
{
    setup();
    int id = soft_timer_create(&mgr, 2, SOFT_TIMER_REPEAT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);

    soft_timer_stop(&mgr, id);
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);
}

TEST(test_reset)
{
    setup();
    int id = soft_timer_create(&mgr, 3, SOFT_TIMER_ONE_SHOT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0);

    soft_timer_reset(&mgr, id);
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);
}

TEST(test_user_ctx)
{
    setup();
    int marker = 42;
    int id = soft_timer_create(&mgr, 1, SOFT_TIMER_ONE_SHOT, test_callback, &marker);
    soft_timer_start(&mgr, id);
    soft_timer_tick(&mgr);
    ASSERT_TRUE(cb_last_ctx == &marker);
}

TEST(test_multiple_timers)
{
    setup();
    int id1 = soft_timer_create(&mgr, 2, SOFT_TIMER_ONE_SHOT, test_callback, NULL);
    int id2 = soft_timer_create(&mgr, 3, SOFT_TIMER_ONE_SHOT, test_callback2, NULL);
    soft_timer_start(&mgr, id1);
    soft_timer_start(&mgr, id2);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0);
    ASSERT_EQ(cb2_count, 0);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(cb2_count, 0);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(cb2_count, 1);
}

TEST(test_invalid_id)
{
    setup();
    ASSERT_EQ(soft_timer_start(&mgr, 0), SOFT_TIMER_ERR_PARAM);
    ASSERT_EQ(soft_timer_stop(&mgr, -1), SOFT_TIMER_ERR_PARAM);
    ASSERT_EQ(soft_timer_reset(&mgr, 99), SOFT_TIMER_ERR_PARAM);
    ASSERT_TRUE(!soft_timer_is_active(&mgr, 5));
}

TEST(test_get_tick)
{
    setup();
    ASSERT_EQ(soft_timer_get_tick(&mgr), 0);
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(soft_timer_get_tick(&mgr), 3);
    ASSERT_EQ(soft_timer_get_tick(NULL), 0);
}

TEST(test_tick_null)
{
    soft_timer_tick(NULL);
    ASSERT_TRUE(1);
}

TEST(test_period_1)
{
    setup();
    int id = soft_timer_create(&mgr, 1, SOFT_TIMER_REPEAT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 2);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 3);
}

TEST(test_soft_timer_err_full_triggered)
{
    /* TODO: trigger SOFT_TIMER_ERR_FULL and assert return value */
}


TEST_MAIN("soft_timer")
    RUN(test_init);
    RUN(test_null_safety);
    RUN(test_create_and_start);
    RUN(test_one_shot_fires);
    RUN(test_repeat_fires);
    RUN(test_stop);
    RUN(test_reset);
    RUN(test_user_ctx);
    RUN(test_multiple_timers);
    RUN(test_invalid_id);
    RUN(test_get_tick);
    RUN(test_tick_null);
    RUN(test_period_1);
    RUN(test_soft_timer_err_full_triggered);
TEST_MAIN_END()

