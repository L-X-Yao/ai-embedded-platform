#include "test_harness.h"
#include <math.h>
#include "moving_average.h"
/* === SMA tests === */

TEST(test_sma_init)
{
    moving_average_sma_t sma;
    float buf[4];
    ASSERT_EQ(moving_average_sma_init(&sma, buf, 4), MOVING_AVERAGE_OK);
    ASSERT_EQ(sma.count, 0);
    ASSERT_TRUE(!moving_average_sma_is_full(&sma));
}

TEST(test_sma_null_safety)
{
    float buf[4];
    ASSERT_EQ(moving_average_sma_init(NULL, buf, 4), MOVING_AVERAGE_ERR_PARAM);
    moving_average_sma_t sma;
    ASSERT_EQ(moving_average_sma_init(&sma, NULL, 4), MOVING_AVERAGE_ERR_PARAM);
    ASSERT_EQ(moving_average_sma_init(&sma, buf, 0), MOVING_AVERAGE_ERR_PARAM);
    ASSERT_EQ(moving_average_sma_reset(NULL), MOVING_AVERAGE_ERR_PARAM);
    ASSERT_NEAR(moving_average_sma_feed(NULL, 1.0f), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_average_sma_get(NULL), 0.0f, 1e-6f);
    ASSERT_TRUE(!moving_average_sma_is_full(NULL));
}

TEST(test_sma_single_sample)
{
    moving_average_sma_t sma;
    float buf[4];
    moving_average_sma_init(&sma, buf, 4);
    float avg = moving_average_sma_feed(&sma, 10.0f);
    ASSERT_NEAR(avg, 10.0f, 1e-6f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 10.0f, 1e-6f);
}

TEST(test_sma_filling)
{
    moving_average_sma_t sma;
    float buf[4];
    moving_average_sma_init(&sma, buf, 4);

    moving_average_sma_feed(&sma, 2.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 2.0f, 1e-6f);

    moving_average_sma_feed(&sma, 4.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 3.0f, 1e-6f);

    moving_average_sma_feed(&sma, 6.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 4.0f, 1e-6f);

    ASSERT_TRUE(!moving_average_sma_is_full(&sma));

    moving_average_sma_feed(&sma, 8.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 5.0f, 1e-6f);
    ASSERT_TRUE(moving_average_sma_is_full(&sma));
}

TEST(test_sma_sliding)
{
    moving_average_sma_t sma;
    float buf[3];
    moving_average_sma_init(&sma, buf, 3);

    moving_average_sma_feed(&sma, 1.0f);
    moving_average_sma_feed(&sma, 2.0f);
    moving_average_sma_feed(&sma, 3.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), 2.0f, 1e-6f);

    moving_average_sma_feed(&sma, 6.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), (2.0f + 3.0f + 6.0f) / 3.0f, 1e-4f);

    moving_average_sma_feed(&sma, 9.0f);
    ASSERT_NEAR(moving_average_sma_get(&sma), (3.0f + 6.0f + 9.0f) / 3.0f, 1e-4f);
}

TEST(test_sma_reset)
{
    moving_average_sma_t sma;
    float buf[4];
    moving_average_sma_init(&sma, buf, 4);
    moving_average_sma_feed(&sma, 100.0f);
    moving_average_sma_feed(&sma, 200.0f);

    moving_average_sma_reset(&sma);
    ASSERT_EQ(sma.count, 0);
    ASSERT_NEAR(sma.sum, 0.0f, 1e-6f);
    ASSERT_TRUE(!moving_average_sma_is_full(&sma));

    float avg = moving_average_sma_feed(&sma, 5.0f);
    ASSERT_NEAR(avg, 5.0f, 1e-6f);
}

TEST(test_sma_constant_input)
{
    moving_average_sma_t sma;
    float buf[8];
    moving_average_sma_init(&sma, buf, 8);
    for (int i = 0; i < 20; i++)
    {
        moving_average_sma_feed(&sma, 42.0f);
    }
    ASSERT_NEAR(moving_average_sma_get(&sma), 42.0f, 1e-4f);
}

TEST(test_sma_window_1)
{
    moving_average_sma_t sma;
    float buf[1];
    moving_average_sma_init(&sma, buf, 1);
    ASSERT_NEAR(moving_average_sma_feed(&sma, 5.0f), 5.0f, 1e-6f);
    ASSERT_NEAR(moving_average_sma_feed(&sma, 10.0f), 10.0f, 1e-6f);
}

TEST(test_sma_get_empty)
{
    moving_average_sma_t sma;
    float buf[4];
    moving_average_sma_init(&sma, buf, 4);
    ASSERT_NEAR(moving_average_sma_get(&sma), 0.0f, 1e-6f);
}

/* === EMA tests === */

TEST(test_ema_init)
{
    moving_average_ema_t ema;
    ASSERT_EQ(moving_average_ema_init(&ema, 0.5f), MOVING_AVERAGE_OK);
    ASSERT_TRUE(!ema.primed);
}

TEST(test_ema_null_safety)
{
    ASSERT_EQ(moving_average_ema_init(NULL, 0.5f), MOVING_AVERAGE_ERR_PARAM);
    moving_average_ema_t ema;
    ASSERT_EQ(moving_average_ema_init(&ema, 0.0f), MOVING_AVERAGE_ERR_PARAM);
    ASSERT_EQ(moving_average_ema_init(&ema, -0.1f), MOVING_AVERAGE_ERR_PARAM);
    ASSERT_EQ(moving_average_ema_init(&ema, 1.1f), MOVING_AVERAGE_ERR_PARAM);
    ASSERT_EQ(moving_average_ema_reset(NULL), MOVING_AVERAGE_ERR_PARAM);
    ASSERT_NEAR(moving_average_ema_feed(NULL, 1.0f), 0.0f, 1e-6f);
    ASSERT_NEAR(moving_average_ema_get(NULL), 0.0f, 1e-6f);
}

TEST(test_ema_first_sample)
{
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.3f);
    float val = moving_average_ema_feed(&ema, 100.0f);
    ASSERT_NEAR(val, 100.0f, 1e-6f);
    ASSERT_TRUE(ema.primed);
}

TEST(test_ema_smoothing)
{
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.5f);
    moving_average_ema_feed(&ema, 0.0f);
    float val = moving_average_ema_feed(&ema, 10.0f);
    ASSERT_NEAR(val, 5.0f, 1e-6f);

    val = moving_average_ema_feed(&ema, 10.0f);
    ASSERT_NEAR(val, 7.5f, 1e-4f);
}

TEST(test_ema_alpha_1)
{
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 1.0f);
    moving_average_ema_feed(&ema, 5.0f);
    float val = moving_average_ema_feed(&ema, 10.0f);
    ASSERT_NEAR(val, 10.0f, 1e-6f);
}

TEST(test_ema_convergence)
{
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.1f);
    moving_average_ema_feed(&ema, 0.0f);
    for (int i = 0; i < 100; i++)
    {
        moving_average_ema_feed(&ema, 50.0f);
    }
    ASSERT_NEAR(moving_average_ema_get(&ema), 50.0f, 0.01f);
}

TEST(test_ema_reset)
{
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.5f);
    moving_average_ema_feed(&ema, 100.0f);
    moving_average_ema_reset(&ema);
    ASSERT_TRUE(!ema.primed);
    float val = moving_average_ema_feed(&ema, 7.0f);
    ASSERT_NEAR(val, 7.0f, 1e-6f);
}

TEST(test_ema_get)
{
    moving_average_ema_t ema;
    moving_average_ema_init(&ema, 0.5f);
    ASSERT_NEAR(moving_average_ema_get(&ema), 0.0f, 1e-6f);
    moving_average_ema_feed(&ema, 42.0f);
    ASSERT_NEAR(moving_average_ema_get(&ema), 42.0f, 1e-6f);
}

TEST_MAIN("moving_average")
    RUN(test_sma_init);
    RUN(test_sma_null_safety);
    RUN(test_sma_single_sample);
    RUN(test_sma_filling);
    RUN(test_sma_sliding);
    RUN(test_sma_reset);
    RUN(test_sma_constant_input);
    RUN(test_sma_window_1);
    RUN(test_sma_get_empty);
    RUN(test_ema_init);
    RUN(test_ema_null_safety);
    RUN(test_ema_first_sample);
    RUN(test_ema_smoothing);
    RUN(test_ema_alpha_1);
    RUN(test_ema_convergence);
    RUN(test_ema_reset);
    RUN(test_ema_get);
TEST_MAIN_END()

