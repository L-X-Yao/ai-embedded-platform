// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "timer_wheel.h"
#include <string.h>

/* ---- helpers ------------------------------------------------------- */

#define NUM_SLOTS   8
#define MAX_TIMERS  8

static timer_wheel_t        tw;
static timer_wheel_slot_t   slots[NUM_SLOTS];
static timer_wheel_entry_t  timers[MAX_TIMERS];

static int  cb_count;
static void *cb_last_ctx;

static void test_cb(void *ctx)
{
    cb_count++;
    cb_last_ctx = ctx;
}

static int cb2_count;
static void test_cb2(void *ctx)
{
    (void)ctx;
    cb2_count++;
}

static void setup(void)
{
    cb_count    = 0;
    cb2_count   = 0;
    cb_last_ctx = NULL;
    timer_wheel_init(&tw, slots, NUM_SLOTS, timers, MAX_TIMERS);
}

/* ---- tests --------------------------------------------------------- */

TEST(test_init)
{
    setup();
    ASSERT_EQ(tw.current_pos, 0);
    ASSERT_EQ(tw.count, 0);
    ASSERT_EQ(tw.num_slots, NUM_SLOTS);
    ASSERT_EQ(tw.max_timers, MAX_TIMERS);
}

TEST(test_null_params)
{
    ASSERT_EQ(timer_wheel_init(NULL, slots, NUM_SLOTS, timers, MAX_TIMERS),
              TIMER_WHEEL_ERR_PARAM);
    ASSERT_EQ(timer_wheel_init(&tw, NULL, NUM_SLOTS, timers, MAX_TIMERS),
              TIMER_WHEEL_ERR_PARAM);
    ASSERT_EQ(timer_wheel_init(&tw, slots, NUM_SLOTS, NULL, MAX_TIMERS),
              TIMER_WHEEL_ERR_PARAM);
    ASSERT_EQ(timer_wheel_init(&tw, slots, 0, timers, MAX_TIMERS),
              TIMER_WHEEL_ERR_PARAM);
    ASSERT_EQ(timer_wheel_init(&tw, slots, NUM_SLOTS, timers, 0),
              TIMER_WHEEL_ERR_PARAM);

    setup();
    ASSERT_EQ(timer_wheel_add(NULL, 0, 5, test_cb, NULL),
              TIMER_WHEEL_ERR_PARAM);
    ASSERT_EQ(timer_wheel_add(&tw, 0, 5, NULL, NULL),
              TIMER_WHEEL_ERR_PARAM);
    ASSERT_EQ(timer_wheel_add(&tw, 0, 0, test_cb, NULL),
              TIMER_WHEEL_ERR_PARAM);
    ASSERT_EQ(timer_wheel_add(&tw, MAX_TIMERS, 5, test_cb, NULL),
              TIMER_WHEEL_ERR_PARAM);

    ASSERT_EQ(timer_wheel_cancel(NULL, 0), TIMER_WHEEL_ERR_PARAM);
    ASSERT_EQ(timer_wheel_cancel(&tw, MAX_TIMERS), TIMER_WHEEL_ERR_PARAM);
    /* cancel inactive timer */
    ASSERT_EQ(timer_wheel_cancel(&tw, 0), TIMER_WHEEL_ERR_PARAM);

    /* tick NULL must not crash */
    timer_wheel_tick(NULL);
    ASSERT_TRUE(1);
}

TEST(test_add_and_expire)
{
    setup();
    ASSERT_EQ(timer_wheel_add(&tw, 0, 3, test_cb, NULL), TIMER_WHEEL_OK);
    ASSERT_EQ(tw.count, 1);

    timer_wheel_tick(&tw);  /* tick 1 */
    ASSERT_EQ(cb_count, 0);
    timer_wheel_tick(&tw);  /* tick 2 */
    ASSERT_EQ(cb_count, 0);
    timer_wheel_tick(&tw);  /* tick 3 — fires */
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(tw.count, 0);

    /* further ticks must not re-fire */
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);
}

TEST(test_cancel)
{
    setup();
    ASSERT_EQ(timer_wheel_add(&tw, 0, 5, test_cb, NULL), TIMER_WHEEL_OK);
    ASSERT_EQ(tw.count, 1);

    timer_wheel_tick(&tw);
    timer_wheel_tick(&tw);
    ASSERT_EQ(timer_wheel_cancel(&tw, 0), TIMER_WHEEL_OK);
    ASSERT_EQ(tw.count, 0);

    /* remaining ticks must not fire */
    timer_wheel_tick(&tw);
    timer_wheel_tick(&tw);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 0);
}

TEST(test_multiple_same_slot)
{
    /*
     * Two timers with the same delay land in the same slot.
     * Both must fire on the same tick.
     */
    setup();
    ASSERT_EQ(timer_wheel_add(&tw, 0, 4, test_cb,  NULL), TIMER_WHEEL_OK);
    ASSERT_EQ(timer_wheel_add(&tw, 1, 4, test_cb2, NULL), TIMER_WHEEL_OK);
    ASSERT_EQ(tw.count, 2);

    for (int i = 0; i < 3; i++)
    {
        timer_wheel_tick(&tw);
    }
    ASSERT_EQ(cb_count, 0);
    ASSERT_EQ(cb2_count, 0);

    timer_wheel_tick(&tw);  /* tick 4 — both fire */
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(cb2_count, 1);
    ASSERT_EQ(tw.count, 0);
}

TEST(test_wraparound)
{
    /*
     * Timer with ticks > num_slots must survive full wheel revolution(s)
     * and fire at the correct tick.  delay = num_slots + 3 = 11.
     */
    setup();
    uint32_t delay = NUM_SLOTS + 3;
    ASSERT_EQ(timer_wheel_add(&tw, 0, delay, test_cb, NULL), TIMER_WHEEL_OK);

    /* tick through one full revolution — must NOT fire */
    for (uint32_t i = 0; i < NUM_SLOTS; i++)
    {
        timer_wheel_tick(&tw);
    }
    ASSERT_EQ(cb_count, 0);

    /* tick remaining 3 — fires at tick 11 */
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 0);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 0);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(tw.count, 0);
}

TEST(test_readd_after_expire)
{
    setup();
    ASSERT_EQ(timer_wheel_add(&tw, 0, 2, test_cb, NULL), TIMER_WHEEL_OK);

    timer_wheel_tick(&tw);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);

    /* re-add same ID after it expired */
    ASSERT_EQ(timer_wheel_add(&tw, 0, 3, test_cb, NULL), TIMER_WHEEL_OK);
    ASSERT_EQ(tw.count, 1);

    timer_wheel_tick(&tw);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 2);
}

/* ---- add and cancel from inside a callback ------------------------- */

typedef struct {
    timer_wheel_t *w;
    uint32_t       id;
    uint32_t       ticks;
} rearm_t;

/* A callback that starts another timer running test_cb. */
static void add_from_cb(void *ctx)
{
    rearm_t *r = ctx;
    timer_wheel_add(r->w, r->id, r->ticks, test_cb, NULL);
}

/* A callback that cancels another timer, then starts it again. */
static void cancel_and_add_from_cb(void *ctx)
{
    rearm_t *r = ctx;
    timer_wheel_cancel(r->w, r->id);
    timer_wheel_add(r->w, r->id, r->ticks, test_cb, NULL);
}

/* A callback that cancels another timer. */
static void cancel_from_cb(void *ctx)
{
    rearm_t *r = ctx;
    timer_wheel_cancel(r->w, r->id);
}

/* A callback that starts itself again. */
static void periodic_cb(void *ctx)
{
    rearm_t *r = ctx;
    cb_count++;
    timer_wheel_add(r->w, r->id, r->ticks, periodic_cb, r);
}

/* The tick number, counting from 1, on which test_cb first ran; 0 if never. */
static uint32_t tick_of_first_fire(timer_wheel_t *w, uint32_t limit)
{
    for (uint32_t t = 1; t <= limit; t++) {
        timer_wheel_tick(w);
        if (cb_count > 0) {
            return t;
        }
    }
    return 0;
}

TEST(test_add_in_cb_full_revolution_higher_id)
{
    /* id 0 fires at tick 1 and adds id 1 for 8 ticks: 1 + 8 = 9. Slot
     * (pos + 8) % 8 is the one being scanned, and id 1 is still ahead. */
    setup();
    rearm_t r = { &tw, 1, 8 };
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, add_from_cb, &r), TIMER_WHEEL_OK);
    ASSERT_EQ(tick_of_first_fire(&tw, 40), 9);
}

TEST(test_add_in_cb_full_revolution_lower_id)
{
    setup();
    rearm_t r = { &tw, 0, 8 };
    ASSERT_EQ(timer_wheel_add(&tw, 1, 1, add_from_cb, &r), TIMER_WHEEL_OK);
    ASSERT_EQ(tick_of_first_fire(&tw, 40), 9);
}

TEST(test_add_in_cb_several_revolutions)
{
    /* 1 + 24 = 25: the scan under way must not count as a revolution. */
    setup();
    rearm_t r = { &tw, 1, 24 };
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, add_from_cb, &r), TIMER_WHEEL_OK);
    ASSERT_EQ(tick_of_first_fire(&tw, 40), 25);
}

TEST(test_add_in_cb_single_slot_wheel)
{
    /* Every timer lives in the one slot: 1 + 8 = 9. */
    timer_wheel_t        w;
    timer_wheel_slot_t   one_slot[1];
    setup();
    ASSERT_EQ(timer_wheel_init(&w, one_slot, 1, timers, MAX_TIMERS),
              TIMER_WHEEL_OK);

    rearm_t r = { &w, 1, 8 };
    ASSERT_EQ(timer_wheel_add(&w, 0, 1, add_from_cb, &r), TIMER_WHEEL_OK);
    ASSERT_EQ(tick_of_first_fire(&w, 40), 9);
}

TEST(test_add_in_cb_other_slot)
{
    /* 1 + 3 = 4, in a slot the scan is not on. */
    setup();
    rearm_t r = { &tw, 1, 3 };
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, add_from_cb, &r), TIMER_WHEEL_OK);
    ASSERT_EQ(tick_of_first_fire(&tw, 40), 4);
}

TEST(test_readd_self_in_cb_stays_periodic)
{
    /* Fires at ticks 1, 9, 17 and 25. */
    setup();
    rearm_t r = { &tw, 0, 8 };
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, periodic_cb, &r), TIMER_WHEEL_OK);

    for (uint32_t t = 1; t <= 25; t++) {
        timer_wheel_tick(&tw);
        ASSERT_EQ((uint32_t)cb_count, (t + 7) / 8);
    }
}

TEST(test_cancel_in_cb_stops_a_timer_due_on_the_same_tick)
{
    setup();
    rearm_t r = { &tw, 1, 0 };
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, cancel_from_cb, &r), TIMER_WHEEL_OK);
    ASSERT_EQ(timer_wheel_add(&tw, 1, 1, test_cb, NULL), TIMER_WHEEL_OK);

    ASSERT_EQ(tick_of_first_fire(&tw, 40), 0);
    ASSERT_EQ(tw.count, 0);
}

TEST(test_cancel_and_add_in_cb_restarts_a_timer_due_on_the_same_tick)
{
    /* id 1 was due at tick 1, but the callback restarts it first: 1 + 8. */
    setup();
    rearm_t r = { &tw, 1, 8 };
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, cancel_and_add_from_cb, &r),
              TIMER_WHEEL_OK);
    ASSERT_EQ(timer_wheel_add(&tw, 1, 1, test_cb2, NULL), TIMER_WHEEL_OK);

    ASSERT_EQ(tick_of_first_fire(&tw, 40), 9);
    ASSERT_EQ(cb2_count, 0);
    ASSERT_EQ(tw.count, 0);
}

TEST(test_readd_after_cancel)
{
    setup();
    ASSERT_EQ(timer_wheel_add(&tw, 0, 5, test_cb, NULL), TIMER_WHEEL_OK);
    ASSERT_EQ(timer_wheel_cancel(&tw, 0), TIMER_WHEEL_OK);

    /* re-add same ID after cancel */
    ASSERT_EQ(timer_wheel_add(&tw, 0, 2, test_cb, NULL), TIMER_WHEEL_OK);
    ASSERT_EQ(tw.count, 1);

    timer_wheel_tick(&tw);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);
}

TEST(test_tick_empty)
{
    setup();
    /* ticking an empty wheel must not crash */
    for (int i = 0; i < 20; i++)
    {
        timer_wheel_tick(&tw);
    }
    ASSERT_EQ(cb_count, 0);
    ASSERT_EQ(tw.count, 0);
}

TEST(test_max_timers)
{
    setup();
    /* fill all timer slots */
    for (uint32_t i = 0; i < MAX_TIMERS; i++)
    {
        ASSERT_EQ(timer_wheel_add(&tw, i, (i + 1) * 2, test_cb, NULL),
                  TIMER_WHEEL_OK);
    }
    ASSERT_EQ(tw.count, MAX_TIMERS);

    /* adding when slot is active returns FULL */
    ASSERT_EQ(timer_wheel_add(&tw, 0, 5, test_cb, NULL),
              TIMER_WHEEL_ERR_FULL);
}

TEST(test_user_ctx_passthrough)
{
    setup();
    int marker = 99;
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, test_cb, &marker), TIMER_WHEEL_OK);

    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);
    ASSERT_TRUE(cb_last_ctx == &marker);
}

TEST(test_delay_equals_num_slots)
{
    /*
     * delay == num_slots means exactly one full revolution.
     * remaining_rounds = num_slots / num_slots = 1, target_slot = current_pos.
     * Must fire after exactly num_slots ticks.
     */
    setup();
    ASSERT_EQ(timer_wheel_add(&tw, 0, NUM_SLOTS, test_cb, NULL), TIMER_WHEEL_OK);

    for (uint32_t i = 0; i < NUM_SLOTS - 1; i++)
    {
        timer_wheel_tick(&tw);
    }
    ASSERT_EQ(cb_count, 0);

    timer_wheel_tick(&tw);  /* exactly NUM_SLOTS ticks */
    ASSERT_EQ(cb_count, 1);
}

TEST(test_delay_one)
{
    setup();
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, test_cb, NULL), TIMER_WHEEL_OK);

    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(tw.count, 0);
}

TEST(test_multiple_different_delays)
{
    setup();
    /* id 0: fires at tick 2, id 1: fires at tick 5 */
    ASSERT_EQ(timer_wheel_add(&tw, 0, 2, test_cb,  NULL), TIMER_WHEEL_OK);
    ASSERT_EQ(timer_wheel_add(&tw, 1, 5, test_cb2, NULL), TIMER_WHEEL_OK);

    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 0);
    ASSERT_EQ(cb2_count, 0);

    timer_wheel_tick(&tw);  /* tick 2 */
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(cb2_count, 0);

    timer_wheel_tick(&tw);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb2_count, 0);

    timer_wheel_tick(&tw);  /* tick 5 */
    ASSERT_EQ(cb2_count, 1);
}

TEST(test_cancel_inactive_returns_err)
{
    setup();
    /* never added — cancel must fail */
    ASSERT_EQ(timer_wheel_cancel(&tw, 0), TIMER_WHEEL_ERR_PARAM);

    /* add, let expire, cancel again must fail */
    ASSERT_EQ(timer_wheel_add(&tw, 0, 1, test_cb, NULL), TIMER_WHEEL_OK);
    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);
    ASSERT_EQ(timer_wheel_cancel(&tw, 0), TIMER_WHEEL_ERR_PARAM);
}

TEST(test_large_wraparound)
{
    /*
     * delay = 3 * num_slots + 2 = 26 ticks with 8 slots.
     * remaining_rounds = 26 / 8 = 3, target_slot = (0 + 26) % 8 = 2.
     * Must fire after exactly 26 ticks.
     */
    setup();
    uint32_t delay = 3 * NUM_SLOTS + 2;
    ASSERT_EQ(timer_wheel_add(&tw, 0, delay, test_cb, NULL), TIMER_WHEEL_OK);

    for (uint32_t i = 0; i < delay - 1; i++)
    {
        timer_wheel_tick(&tw);
    }
    ASSERT_EQ(cb_count, 0);

    timer_wheel_tick(&tw);
    ASSERT_EQ(cb_count, 1);
}

/* ---- main ---------------------------------------------------------- */

/* add and cancel refuse a wheel that was never set up: so must tick. */
TEST(test_tick_on_zeroed_wheel)
{
    timer_wheel_t zero;

    memset(&zero, 0, sizeof(zero));
    timer_wheel_tick(&zero);
    ASSERT_EQ(zero.current_pos, 0u);
}

TEST(test_tick_after_failed_init)
{
    timer_wheel_t failed;

    memset(&failed, 0, sizeof(failed));
    ASSERT_EQ(timer_wheel_init(&failed, slots, 0, timers, MAX_TIMERS),
              TIMER_WHEEL_ERR_PARAM);
    timer_wheel_tick(&failed);
    ASSERT_EQ(failed.current_pos, 0u);
}

TEST_MAIN("timer_wheel")
    RUN(test_init);
    RUN(test_null_params);
    RUN(test_add_and_expire);
    RUN(test_cancel);
    RUN(test_multiple_same_slot);
    RUN(test_wraparound);
    RUN(test_readd_after_expire);
    RUN(test_add_in_cb_full_revolution_higher_id);
    RUN(test_add_in_cb_full_revolution_lower_id);
    RUN(test_add_in_cb_several_revolutions);
    RUN(test_add_in_cb_single_slot_wheel);
    RUN(test_add_in_cb_other_slot);
    RUN(test_readd_self_in_cb_stays_periodic);
    RUN(test_cancel_in_cb_stops_a_timer_due_on_the_same_tick);
    RUN(test_cancel_and_add_in_cb_restarts_a_timer_due_on_the_same_tick);
    RUN(test_readd_after_cancel);
    RUN(test_tick_empty);
    RUN(test_max_timers);
    RUN(test_user_ctx_passthrough);
    RUN(test_delay_equals_num_slots);
    RUN(test_delay_one);
    RUN(test_multiple_different_delays);
    RUN(test_cancel_inactive_returns_err);
    RUN(test_large_wraparound);
    RUN(test_tick_on_zeroed_wheel);
    RUN(test_tick_after_failed_init);
TEST_MAIN_END()
