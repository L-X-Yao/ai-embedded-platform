#include <stdio.h>
#include "../soft_timer.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((int)(a) == (int)(b), msg)

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

static void test_init(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(mgr.count, 0, "no timers");
    ASSERT_EQ(mgr.tick_count, 0, "tick zero");
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    ASSERT_EQ(soft_timer_init(NULL), SOFT_TIMER_ERR_PARAM, "init null");
    ASSERT_EQ(soft_timer_create(NULL, 10, SOFT_TIMER_ONE_SHOT, test_callback, NULL),
              SOFT_TIMER_ERR_PARAM, "create null mgr");
    setup();
    ASSERT_EQ(soft_timer_create(&mgr, 0, SOFT_TIMER_ONE_SHOT, test_callback, NULL),
              SOFT_TIMER_ERR_PARAM, "create period=0");
    ASSERT_EQ(soft_timer_create(&mgr, 10, SOFT_TIMER_ONE_SHOT, NULL, NULL),
              SOFT_TIMER_ERR_PARAM, "create null cb");
    ASSERT_EQ(soft_timer_start(NULL, 0), SOFT_TIMER_ERR_PARAM, "start null");
    ASSERT_EQ(soft_timer_stop(NULL, 0), SOFT_TIMER_ERR_PARAM, "stop null");
    ASSERT_EQ(soft_timer_reset(NULL, 0), SOFT_TIMER_ERR_PARAM, "reset null");
    ASSERT(!soft_timer_is_active(NULL, 0), "is_active null");
}

static void test_create_and_start(void)
{
    printf("%s\n", __func__);
    setup();
    int id = soft_timer_create(&mgr, 5, SOFT_TIMER_ONE_SHOT, test_callback, NULL);
    ASSERT(id >= 0, "create returns valid id");
    ASSERT_EQ(mgr.count, 1, "count incremented");
    ASSERT(!soft_timer_is_active(&mgr, id), "not active before start");

    soft_timer_start(&mgr, id);
    ASSERT(soft_timer_is_active(&mgr, id), "active after start");
}

static void test_one_shot_fires(void)
{
    printf("%s\n", __func__);
    setup();
    int id = soft_timer_create(&mgr, 3, SOFT_TIMER_ONE_SHOT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0, "tick 1: not fired");
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0, "tick 2: not fired");
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "tick 3: fired");
    ASSERT(!soft_timer_is_active(&mgr, id), "one-shot auto-stops");

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "tick 4: still 1 (stopped)");
}

static void test_repeat_fires(void)
{
    printf("%s\n", __func__);
    setup();
    int id = soft_timer_create(&mgr, 2, SOFT_TIMER_REPEAT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "first fire at tick 2");

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 2, "second fire at tick 4");

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 3, "third fire at tick 6");
    ASSERT(soft_timer_is_active(&mgr, id), "repeat stays active");
}

static void test_stop(void)
{
    printf("%s\n", __func__);
    setup();
    int id = soft_timer_create(&mgr, 2, SOFT_TIMER_REPEAT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "fired once");

    soft_timer_stop(&mgr, id);
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "no more fires after stop");
}

static void test_reset(void)
{
    printf("%s\n", __func__);
    setup();
    int id = soft_timer_create(&mgr, 3, SOFT_TIMER_ONE_SHOT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0, "not fired yet");

    soft_timer_reset(&mgr, id);
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0, "still not fired after reset");

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "fires after full period from reset");
}

static void test_user_ctx(void)
{
    printf("%s\n", __func__);
    setup();
    int marker = 42;
    int id = soft_timer_create(&mgr, 1, SOFT_TIMER_ONE_SHOT, test_callback, &marker);
    soft_timer_start(&mgr, id);
    soft_timer_tick(&mgr);
    ASSERT(cb_last_ctx == &marker, "user_ctx passed to callback");
}

static void test_multiple_timers(void)
{
    printf("%s\n", __func__);
    setup();
    int id1 = soft_timer_create(&mgr, 2, SOFT_TIMER_ONE_SHOT, test_callback, NULL);
    int id2 = soft_timer_create(&mgr, 3, SOFT_TIMER_ONE_SHOT, test_callback2, NULL);
    soft_timer_start(&mgr, id1);
    soft_timer_start(&mgr, id2);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 0, "tick 1: neither");
    ASSERT_EQ(cb2_count, 0, "tick 1: neither");

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "tick 2: timer1 fires");
    ASSERT_EQ(cb2_count, 0, "tick 2: timer2 not yet");

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "tick 3: timer1 done");
    ASSERT_EQ(cb2_count, 1, "tick 3: timer2 fires");
}

static void test_invalid_id(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(soft_timer_start(&mgr, 0), SOFT_TIMER_ERR_PARAM, "start invalid id");
    ASSERT_EQ(soft_timer_stop(&mgr, -1), SOFT_TIMER_ERR_PARAM, "stop negative id");
    ASSERT_EQ(soft_timer_reset(&mgr, 99), SOFT_TIMER_ERR_PARAM, "reset out of range");
    ASSERT(!soft_timer_is_active(&mgr, 5), "is_active invalid");
}

static void test_get_tick(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(soft_timer_get_tick(&mgr), 0, "tick starts at 0");
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    soft_timer_tick(&mgr);
    ASSERT_EQ(soft_timer_get_tick(&mgr), 3, "tick incremented");
    ASSERT_EQ(soft_timer_get_tick(NULL), 0, "get_tick null");
}

static void test_tick_null(void)
{
    printf("%s\n", __func__);
    soft_timer_tick(NULL);
    ASSERT(1, "tick null doesn't crash");
}

static void test_period_1(void)
{
    printf("%s\n", __func__);
    setup();
    int id = soft_timer_create(&mgr, 1, SOFT_TIMER_REPEAT, test_callback, NULL);
    soft_timer_start(&mgr, id);

    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 1, "period=1: fires every tick");
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 2, "period=1: fires again");
    soft_timer_tick(&mgr);
    ASSERT_EQ(cb_count, 3, "period=1: fires again");
}

int main(void)
{
    printf("test_soft_timer\n");
    test_init();
    test_null_safety();
    test_create_and_start();
    test_one_shot_fires();
    test_repeat_fires();
    test_stop();
    test_reset();
    test_user_ctx();
    test_multiple_timers();
    test_invalid_id();
    test_get_tick();
    test_tick_null();
    test_period_1();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
