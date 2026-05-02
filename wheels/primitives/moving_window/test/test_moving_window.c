#include "test_harness.h"
#include <string.h>
#include "moving_window.h"

/* ---- NULL / invalid param tests ---- */

TEST(test_init_null_ctx)
{
    float buf[4];
    ASSERT_EQ(moving_window_init(NULL, buf, 4), MOVING_WINDOW_ERR_PARAM);
}

TEST(test_init_null_buf)
{
    moving_window_t ctx;
    ASSERT_EQ(moving_window_init(&ctx, NULL, 4), MOVING_WINDOW_ERR_PARAM);
}

TEST(test_init_zero_capacity)
{
    moving_window_t ctx;
    float buf[1];
    ASSERT_EQ(moving_window_init(&ctx, buf, 0), MOVING_WINDOW_ERR_PARAM);
}

TEST(test_push_null_ctx)
{
    ASSERT_EQ(moving_window_push(NULL, 1.0f), MOVING_WINDOW_ERR_PARAM);
}

TEST(test_reset_null_ctx)
{
    ASSERT_EQ(moving_window_reset(NULL), MOVING_WINDOW_ERR_PARAM);
}

/* ---- empty window behaviour ---- */

TEST(test_empty_returns_zero)
{
    moving_window_t ctx;
    float buf[4];
    moving_window_init(&ctx, buf, 4);

    ASSERT_NEAR(moving_window_get_mean(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_variance(&ctx), 0.0f, 1e-6f);
    ASSERT_EQ(moving_window_get_count(&ctx), 0);
    ASSERT_FALSE(moving_window_is_full(&ctx));
}

/* ---- partial window (not yet full) ---- */

TEST(test_partial_window_stats)
{
    moving_window_t ctx;
    float buf[8];
    moving_window_init(&ctx, buf, 8);

    /* push 3 values into window of 8 */
    moving_window_push(&ctx, 2.0f);
    moving_window_push(&ctx, 4.0f);
    moving_window_push(&ctx, 6.0f);

    ASSERT_EQ(moving_window_get_count(&ctx), 3);
    ASSERT_FALSE(moving_window_is_full(&ctx));

    /* mean = (2+4+6)/3 = 4.0 */
    ASSERT_NEAR(moving_window_get_mean(&ctx), 4.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 2.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 6.0f, 1e-5f);

    /* variance = ((4+16+36)/3) - 16 = 18.666-16 = 2.666... */
    ASSERT_NEAR(moving_window_get_variance(&ctx), 2.6667f, 1e-3f);
}

/* ---- basic stats on full window ---- */

TEST(test_basic_stats_full)
{
    moving_window_t ctx;
    float buf[4];
    moving_window_init(&ctx, buf, 4);

    moving_window_push(&ctx, 1.0f);
    moving_window_push(&ctx, 2.0f);
    moving_window_push(&ctx, 3.0f);
    moving_window_push(&ctx, 4.0f);

    ASSERT_TRUE(moving_window_is_full(&ctx));
    ASSERT_EQ(moving_window_get_count(&ctx), 4);

    /* mean = 2.5 */
    ASSERT_NEAR(moving_window_get_mean(&ctx), 2.5f, 1e-5f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 1.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 4.0f, 1e-5f);

    /* variance = ((1+4+9+16)/4) - 6.25 = 7.5-6.25 = 1.25 */
    ASSERT_NEAR(moving_window_get_variance(&ctx), 1.25f, 1e-5f);
}

/* ---- rollover: oldest evicted ---- */

TEST(test_rollover_evicts_oldest)
{
    moving_window_t ctx;
    float buf[3];
    moving_window_init(&ctx, buf, 3);

    moving_window_push(&ctx, 10.0f);
    moving_window_push(&ctx, 20.0f);
    moving_window_push(&ctx, 30.0f);
    /* window: [10, 20, 30], full */
    ASSERT_TRUE(moving_window_is_full(&ctx));

    /* push 40 — evicts 10 → window: [20, 30, 40] */
    moving_window_push(&ctx, 40.0f);
    ASSERT_EQ(moving_window_get_count(&ctx), 3);
    ASSERT_NEAR(moving_window_get_mean(&ctx), 30.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 20.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 40.0f, 1e-5f);
}

/* ---- min/max tracking across multiple rollovers ---- */

TEST(test_min_max_across_rollover)
{
    moving_window_t ctx;
    float buf[3];
    moving_window_init(&ctx, buf, 3);

    /* fill: [1, 5, 3] */
    moving_window_push(&ctx, 1.0f);
    moving_window_push(&ctx, 5.0f);
    moving_window_push(&ctx, 3.0f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 1.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 5.0f, 1e-5f);

    /* push 2 → evicts 1 → [5, 3, 2] */
    moving_window_push(&ctx, 2.0f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 2.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 5.0f, 1e-5f);

    /* push 4 → evicts 5 → [3, 2, 4] */
    moving_window_push(&ctx, 4.0f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 2.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 4.0f, 1e-5f);
}

/* ---- variance on constant values ---- */

TEST(test_variance_constant)
{
    moving_window_t ctx;
    float buf[5];
    moving_window_init(&ctx, buf, 5);

    for (int i = 0; i < 5; i++)
    {
        moving_window_push(&ctx, 7.0f);
    }

    ASSERT_NEAR(moving_window_get_variance(&ctx), 0.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_mean(&ctx), 7.0f, 1e-5f);
}

/* ---- variance after rollover ---- */

TEST(test_variance_after_rollover)
{
    moving_window_t ctx;
    float buf[3];
    moving_window_init(&ctx, buf, 3);

    /* fill with [1, 2, 3] */
    moving_window_push(&ctx, 1.0f);
    moving_window_push(&ctx, 2.0f);
    moving_window_push(&ctx, 3.0f);

    /* push 4 → [2, 3, 4] */
    moving_window_push(&ctx, 4.0f);

    /* mean = 3.0, variance = ((4+9+16)/3) - 9 = 9.667-9 = 0.667 */
    ASSERT_NEAR(moving_window_get_mean(&ctx), 3.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_variance(&ctx), 0.6667f, 1e-3f);
}

/* ---- reset clears everything ---- */

TEST(test_reset)
{
    moving_window_t ctx;
    float buf[4];
    moving_window_init(&ctx, buf, 4);

    moving_window_push(&ctx, 10.0f);
    moving_window_push(&ctx, 20.0f);
    moving_window_push(&ctx, 30.0f);

    ASSERT_EQ(moving_window_reset(&ctx), MOVING_WINDOW_OK);
    ASSERT_EQ(moving_window_get_count(&ctx), 0);
    ASSERT_FALSE(moving_window_is_full(&ctx));
    ASSERT_NEAR(moving_window_get_mean(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_variance(&ctx), 0.0f, 1e-6f);

    /* can push after reset */
    moving_window_push(&ctx, 5.0f);
    ASSERT_EQ(moving_window_get_count(&ctx), 1);
    ASSERT_NEAR(moving_window_get_mean(&ctx), 5.0f, 1e-5f);
}

/* ---- window of size 1 ---- */

TEST(test_window_size_one)
{
    moving_window_t ctx;
    float buf[1];
    moving_window_init(&ctx, buf, 1);

    moving_window_push(&ctx, 42.0f);
    ASSERT_TRUE(moving_window_is_full(&ctx));
    ASSERT_NEAR(moving_window_get_mean(&ctx), 42.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 42.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 42.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_variance(&ctx), 0.0f, 1e-5f);

    /* replace with different value */
    moving_window_push(&ctx, -3.0f);
    ASSERT_NEAR(moving_window_get_mean(&ctx), -3.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_min(&ctx), -3.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), -3.0f, 1e-5f);
}

/* ---- negative values ---- */

TEST(test_negative_values)
{
    moving_window_t ctx;
    float buf[4];
    moving_window_init(&ctx, buf, 4);

    moving_window_push(&ctx, -5.0f);
    moving_window_push(&ctx, -10.0f);
    moving_window_push(&ctx, -3.0f);
    moving_window_push(&ctx, -8.0f);

    /* mean = -6.5 */
    ASSERT_NEAR(moving_window_get_mean(&ctx), -6.5f, 1e-5f);
    ASSERT_NEAR(moving_window_get_min(&ctx), -10.0f, 1e-5f);
    ASSERT_NEAR(moving_window_get_max(&ctx), -3.0f, 1e-5f);
}

/* ---- zero-init struct must not crash ---- */

TEST(test_zero_init_safety)
{
    moving_window_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* all queries on zero-init should return safe defaults, not crash */
    ASSERT_NEAR(moving_window_get_mean(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_min(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_max(&ctx), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_window_get_variance(&ctx), 0.0f, 1e-6f);
    ASSERT_EQ(moving_window_get_count(&ctx), 0);
    ASSERT_FALSE(moving_window_is_full(&ctx));
    /* push on zero-init (buf is NULL) returns error */
    ASSERT_EQ(moving_window_push(&ctx, 1.0f), MOVING_WINDOW_ERR_PARAM);
}

TEST_MAIN("moving_window")
    RUN(test_init_null_ctx);
    RUN(test_init_null_buf);
    RUN(test_init_zero_capacity);
    RUN(test_push_null_ctx);
    RUN(test_reset_null_ctx);
    RUN(test_empty_returns_zero);
    RUN(test_partial_window_stats);
    RUN(test_basic_stats_full);
    RUN(test_rollover_evicts_oldest);
    RUN(test_min_max_across_rollover);
    RUN(test_variance_constant);
    RUN(test_variance_after_rollover);
    RUN(test_reset);
    RUN(test_window_size_one);
    RUN(test_negative_values);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
