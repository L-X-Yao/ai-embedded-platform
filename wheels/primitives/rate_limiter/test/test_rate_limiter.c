#include "test_harness.h"
#include "rate_limiter.h"

#include <string.h>

/* --- 1. init ------------------------------------------------------- */
TEST(test_init_ok)
{
    rate_limiter_t rl;
    memset(&rl, 0xAA, sizeof(rl));
    ASSERT_EQ(rate_limiter_init(&rl, 5, 1), RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_available(&rl), 5);
    ASSERT_EQ(rate_limiter_allowed(&rl), 0);
    ASSERT_EQ(rate_limiter_denied(&rl), 0);
}

/* --- 2. try once (allowed) ----------------------------------------- */
TEST(test_try_once_allowed)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 5, 1);
    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_available(&rl), 4);
    ASSERT_EQ(rate_limiter_allowed(&rl), 1);
}

/* --- 3. exhaust tokens (all denied after) -------------------------- */
TEST(test_exhaust_then_denied)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 3, 1);

    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_OK);
    /* bucket empty */
    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_DENIED);
    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_DENIED);
    ASSERT_EQ(rate_limiter_available(&rl), 0);
}

/* --- 4. refill over time ------------------------------------------- */
TEST(test_refill_over_time)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 3, 1); /* 1 token per tick, max 3 */

    /* exhaust */
    rate_limiter_try(&rl, 0);
    rate_limiter_try(&rl, 0);
    rate_limiter_try(&rl, 0);
    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_DENIED);

    /* advance 2 ticks → 2 tokens refilled */
    ASSERT_EQ(rate_limiter_try(&rl, 2), RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_available(&rl), 1); /* 2 refilled - 1 consumed */
}

/* --- 5. try_n (bulk consume) --------------------------------------- */
TEST(test_try_n_bulk)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 10, 1);

    ASSERT_EQ(rate_limiter_try_n(&rl, 0, 5), RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_available(&rl), 5);
    ASSERT_EQ(rate_limiter_allowed(&rl), 1);

    /* try to consume 6 — only 5 left, must deny (no partial) */
    ASSERT_EQ(rate_limiter_try_n(&rl, 0, 6), RATE_LIMITER_DENIED);
    ASSERT_EQ(rate_limiter_available(&rl), 5); /* unchanged */
    ASSERT_EQ(rate_limiter_denied(&rl), 1);

    /* consume exactly 5 */
    ASSERT_EQ(rate_limiter_try_n(&rl, 0, 5), RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_available(&rl), 0);
}

/* --- 6. can_proceed without consuming ------------------------------ */
TEST(test_can_proceed_no_consume)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 2, 1);

    ASSERT_TRUE(rate_limiter_can_proceed(&rl, 0));
    ASSERT_EQ(rate_limiter_available(&rl), 2); /* not consumed */

    /* exhaust */
    rate_limiter_try(&rl, 0);
    rate_limiter_try(&rl, 0);
    ASSERT_FALSE(rate_limiter_can_proceed(&rl, 0));

    /* after refill, can_proceed should return true again */
    ASSERT_TRUE(rate_limiter_can_proceed(&rl, 1));
    /* but actual tokens unchanged (can_proceed is read-only) */
    ASSERT_EQ(rate_limiter_available(&rl), 0);
}

/* --- 7. stats (allowed/denied counts) ------------------------------ */
TEST(test_stats_counts)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 2, 1);

    rate_limiter_try(&rl, 0); /* OK */
    rate_limiter_try(&rl, 0); /* OK */
    rate_limiter_try(&rl, 0); /* DENIED */
    rate_limiter_try(&rl, 0); /* DENIED */
    rate_limiter_try(&rl, 0); /* DENIED */

    ASSERT_EQ(rate_limiter_allowed(&rl), 2);
    ASSERT_EQ(rate_limiter_denied(&rl), 3);
}

/* --- 8. reset ------------------------------------------------------ */
TEST(test_reset)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 3, 1);

    rate_limiter_try(&rl, 0);
    rate_limiter_try(&rl, 0);
    rate_limiter_try(&rl, 0);
    rate_limiter_try(&rl, 0); /* denied */
    ASSERT_EQ(rate_limiter_allowed(&rl), 3);
    ASSERT_EQ(rate_limiter_denied(&rl), 1);

    rate_limiter_reset(&rl);
    ASSERT_EQ(rate_limiter_available(&rl), 3);
    ASSERT_EQ(rate_limiter_allowed(&rl), 0);
    ASSERT_EQ(rate_limiter_denied(&rl), 0);

    /* works again after reset */
    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_OK);
}

/* --- 9. max tokens cap (refill doesn't exceed bucket) -------------- */
TEST(test_max_cap)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 3, 2); /* refill 2/tick, max 3 */

    /* consume 1, then advance 100 ticks — should cap at 3 */
    rate_limiter_try(&rl, 0);
    ASSERT_EQ(rate_limiter_available(&rl), 2);

    /* trigger refill with big gap */
    ASSERT_EQ(rate_limiter_try(&rl, 100), RATE_LIMITER_OK);
    /* max is 3, consumed 1, so 2 available */
    ASSERT_EQ(rate_limiter_available(&rl), 2);
}

/* --- 10. zero rate (init rejects) ---------------------------------- */
TEST(test_zero_rate_rejected)
{
    rate_limiter_t rl;
    ASSERT_EQ(rate_limiter_init(&rl, 5, 0), RATE_LIMITER_ERR_PARAM);
    ASSERT_EQ(rate_limiter_init(&rl, 0, 1), RATE_LIMITER_ERR_PARAM);
    ASSERT_EQ(rate_limiter_init(&rl, 0, 0), RATE_LIMITER_ERR_PARAM);
}

/* --- 11. large time gap (overflow-safe refill) --------------------- */
TEST(test_large_time_gap)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 5, 1);

    /* exhaust */
    for (int i = 0; i < 5; i++) {
        rate_limiter_try(&rl, 0);
    }
    ASSERT_EQ(rate_limiter_available(&rl), 0);

    /* jump to near-max tick — unsigned wraparound should work */
    ASSERT_EQ(rate_limiter_try(&rl, 0xFFFFFFF0U), RATE_LIMITER_OK);
    ASSERT_EQ(rate_limiter_available(&rl), 4); /* refilled to 5, consumed 1 */
}

/* --- 12. NULL params ----------------------------------------------- */
TEST(test_null_safety)
{
    ASSERT_EQ(rate_limiter_init(NULL, 5, 1), RATE_LIMITER_ERR_PARAM);
    ASSERT_EQ(rate_limiter_try(NULL, 0), RATE_LIMITER_ERR_PARAM);
    ASSERT_EQ(rate_limiter_try_n(NULL, 0, 1), RATE_LIMITER_ERR_PARAM);
    ASSERT_FALSE(rate_limiter_can_proceed(NULL, 0));
    rate_limiter_reset(NULL); /* must not crash */
    ASSERT_EQ(rate_limiter_allowed(NULL), 0);
    ASSERT_EQ(rate_limiter_denied(NULL), 0);
    ASSERT_EQ(rate_limiter_available(NULL), 0);
}

/* --- 13. initial tokens equal max ---------------------------------- */
TEST(test_initial_tokens_eq_max)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 7, 2);
    /* tokens start full */
    ASSERT_EQ(rate_limiter_available(&rl), 7);

    /* consume all 7 */
    for (int i = 0; i < 7; i++) {
        ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_OK);
    }
    ASSERT_EQ(rate_limiter_available(&rl), 0);
    ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_DENIED);
}

/* --- 14. available count tracks correctly -------------------------- */
TEST(test_available_tracks)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 4, 1);

    ASSERT_EQ(rate_limiter_available(&rl), 4);
    rate_limiter_try(&rl, 0);
    ASSERT_EQ(rate_limiter_available(&rl), 3);
    rate_limiter_try(&rl, 0);
    ASSERT_EQ(rate_limiter_available(&rl), 2);
    rate_limiter_try_n(&rl, 0, 2);
    ASSERT_EQ(rate_limiter_available(&rl), 0);
}

/* --- 15. try_n with n=0 always succeeds ---------------------------- */
TEST(test_try_n_zero)
{
    rate_limiter_t rl;
    rate_limiter_init(&rl, 1, 1);

    /* even when bucket is empty, n=0 should succeed */
    rate_limiter_try(&rl, 0);
    ASSERT_EQ(rate_limiter_available(&rl), 0);
    ASSERT_EQ(rate_limiter_try_n(&rl, 0, 0), RATE_LIMITER_OK);
}

/* --- 16. zero-init struct ------------------------------------------ */
TEST(test_zero_init_safety)
{
    /* Zero-init struct must not crash when passed to API */
    rate_limiter_t rl;
    memset(&rl, 0, sizeof(rl));
    /* All accessors should handle a zero-init struct gracefully */
    ASSERT_EQ(rate_limiter_available(&rl), 0);
    ASSERT_EQ(rate_limiter_allowed(&rl), 0);
    ASSERT_EQ(rate_limiter_denied(&rl), 0);
}

TEST_MAIN("rate_limiter")
    RUN(test_init_ok);
    RUN(test_try_once_allowed);
    RUN(test_exhaust_then_denied);
    RUN(test_refill_over_time);
    RUN(test_try_n_bulk);
    RUN(test_can_proceed_no_consume);
    RUN(test_stats_counts);
    RUN(test_reset);
    RUN(test_max_cap);
    RUN(test_zero_rate_rejected);
    RUN(test_large_time_gap);
    RUN(test_null_safety);
    RUN(test_initial_tokens_eq_max);
    RUN(test_available_tracks);
    RUN(test_try_n_zero);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
