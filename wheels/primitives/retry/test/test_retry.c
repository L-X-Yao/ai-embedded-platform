// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "retry.h"

TEST(test_fixed_strategy)
{
    retry_config_t cfg = {
        .strategy = RETRY_FIXED,
        .base_delay_ms = 100,
        .max_delay_ms = 1000,
        .max_attempts = 3
    };
    retry_t ctx;
    retry_init(&ctx, &cfg);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 100);
    ASSERT_EQ(retry_get_attempt(&ctx), 1);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 100);
    ASSERT_EQ(retry_get_attempt(&ctx), 2);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 100);
    ASSERT_EQ(retry_get_attempt(&ctx), 3);

    ASSERT_FALSE(retry_should_retry(&ctx));
}

TEST(test_linear_strategy)
{
    retry_config_t cfg = {
        .strategy = RETRY_LINEAR,
        .base_delay_ms = 100,
        .max_delay_ms = 500,
        .max_attempts = 5
    };
    retry_t ctx;
    retry_init(&ctx, &cfg);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 100);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 200);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 300);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 400);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 500);

    ASSERT_FALSE(retry_should_retry(&ctx));
}

TEST(test_exponential_strategy)
{
    retry_config_t cfg = {
        .strategy = RETRY_EXPONENTIAL,
        .base_delay_ms = 100,
        .max_delay_ms = 5000,
        .max_attempts = 5
    };
    retry_t ctx;
    retry_init(&ctx, &cfg);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 100);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 200);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 400);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 800);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_delay(&ctx), 1600);

    ASSERT_FALSE(retry_should_retry(&ctx));
}

TEST(test_max_delay_cap)
{
    retry_config_t cfg = {
        .strategy = RETRY_EXPONENTIAL,
        .base_delay_ms = 1000,
        .max_delay_ms = 3000,
        .max_attempts = 10
    };
    retry_t ctx;
    retry_init(&ctx, &cfg);

    retry_should_retry(&ctx);
    ASSERT_EQ(retry_get_delay(&ctx), 1000);
    retry_should_retry(&ctx);
    ASSERT_EQ(retry_get_delay(&ctx), 2000);
    retry_should_retry(&ctx);
    ASSERT_EQ(retry_get_delay(&ctx), 3000);
    retry_should_retry(&ctx);
    ASSERT_EQ(retry_get_delay(&ctx), 3000);
}

TEST(test_linear_cap)
{
    retry_config_t cfg = {
        .strategy = RETRY_LINEAR,
        .base_delay_ms = 200,
        .max_delay_ms = 500,
        .max_attempts = 5
    };
    retry_t ctx;
    retry_init(&ctx, &cfg);

    retry_should_retry(&ctx);
    ASSERT_EQ(retry_get_delay(&ctx), 200);
    retry_should_retry(&ctx);
    ASSERT_EQ(retry_get_delay(&ctx), 400);
    retry_should_retry(&ctx);
    ASSERT_EQ(retry_get_delay(&ctx), 500);
}

TEST(test_reset)
{
    retry_config_t cfg = {
        .strategy = RETRY_FIXED,
        .base_delay_ms = 50,
        .max_delay_ms = 1000,
        .max_attempts = 2
    };
    retry_t ctx;
    retry_init(&ctx, &cfg);

    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_FALSE(retry_should_retry(&ctx));

    retry_reset(&ctx);
    ASSERT_EQ(retry_get_attempt(&ctx), 0);
    ASSERT_TRUE(retry_should_retry(&ctx));
    ASSERT_EQ(retry_get_attempt(&ctx), 1);
}

TEST(test_zero_attempts)
{
    retry_config_t cfg = {
        .strategy = RETRY_FIXED,
        .base_delay_ms = 100,
        .max_delay_ms = 1000,
        .max_attempts = 0
    };
    retry_t ctx;
    retry_init(&ctx, &cfg);
    ASSERT_FALSE(retry_should_retry(&ctx));
}

TEST(test_null_safety)
{
    retry_config_t cfg = {
        .strategy = RETRY_FIXED,
        .base_delay_ms = 100,
        .max_delay_ms = 1000,
        .max_attempts = 3
    };
    retry_init(NULL, &cfg);
    retry_t ctx;
    retry_init(&ctx, NULL);
    ASSERT_FALSE(retry_should_retry(NULL));
    ASSERT_EQ(retry_get_delay(NULL), 0);
    ASSERT_EQ(retry_get_attempt(NULL), 0);
    retry_reset(NULL);
}

TEST_MAIN("retry")
    RUN(test_fixed_strategy);
    RUN(test_linear_strategy);
    RUN(test_exponential_strategy);
    RUN(test_max_delay_cap);
    RUN(test_linear_cap);
    RUN(test_reset);
    RUN(test_zero_attempts);
    RUN(test_null_safety);
TEST_MAIN_END()
