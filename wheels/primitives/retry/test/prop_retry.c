// SPDX-License-Identifier: MIT
/*
 * prop_retry.c — property-based tests for retry
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_retry.c ../retry.c -o prop_retry
 */

#include "test_harness.h"
#include "property_test.h"
#include "../retry.h"

/* Property: max retries respected — should_retry returns false after max_attempts */
PROPERTY(max_retries_respected, 10000) {
    retry_t ctx;
    uint8_t max = (uint8_t)ARBI_RANGE(seed, 1, 20);
    uint32_t base = (uint32_t)ARBI_RANGE(seed, 10, 500);
    retry_config_t cfg = {
        .strategy      = RETRY_FIXED,
        .base_delay_ms = base,
        .max_delay_ms  = base * 10,
        .max_attempts  = max,
    };
    retry_init(&ctx, &cfg);

    int count = 0;
    while (retry_should_retry(&ctx)) {
        count++;
    }

    PROP_ASSERT_EQ(count, max);
}

/* Property: reset clears state — after reset, full attempts available again */
PROPERTY(reset_clears_state, 10000) {
    retry_t ctx;
    uint8_t max = (uint8_t)ARBI_RANGE(seed, 1, 10);
    retry_config_t cfg = {
        .strategy      = RETRY_EXPONENTIAL,
        .base_delay_ms = 100,
        .max_delay_ms  = 10000,
        .max_attempts  = max,
    };
    retry_init(&ctx, &cfg);

    /* Exhaust some or all attempts */
    unsigned use_count = (unsigned)ARBI_RANGE(seed, 1, max);
    for (unsigned i = 0; i < use_count; i++) {
        retry_should_retry(&ctx);
    }

    /* Reset */
    retry_reset(&ctx);
    PROP_ASSERT_EQ(retry_get_attempt(&ctx), 0);

    /* Should have full attempts available again */
    int count = 0;
    while (retry_should_retry(&ctx)) {
        count++;
    }
    PROP_ASSERT_EQ(count, max);
}

/* Property: delay never exceeds max_delay_ms */
PROPERTY(delay_bounded, 10000) {
    retry_t ctx;
    uint8_t max = (uint8_t)ARBI_RANGE(seed, 1, 15);
    uint32_t base = (uint32_t)ARBI_RANGE(seed, 1, 100);
    uint32_t max_delay = (uint32_t)ARBI_RANGE(seed, (int)base, 10000);
    int strat = ARBI_RANGE(seed, 0, 2);
    retry_config_t cfg = {
        .strategy      = (retry_strategy_t)strat,
        .base_delay_ms = base,
        .max_delay_ms  = max_delay,
        .max_attempts  = max,
    };
    retry_init(&ctx, &cfg);

    while (retry_should_retry(&ctx)) {
        uint32_t delay = retry_get_delay(&ctx);
        PROP_ASSERT(delay <= max_delay);
    }
}

/* Property: attempt counter is monotonically increasing */
PROPERTY(attempt_monotonic, 5000) {
    retry_t ctx;
    uint8_t max = (uint8_t)ARBI_RANGE(seed, 2, 20);
    retry_config_t cfg = {
        .strategy      = RETRY_LINEAR,
        .base_delay_ms = 50,
        .max_delay_ms  = 5000,
        .max_attempts  = max,
    };
    retry_init(&ctx, &cfg);

    uint8_t prev = retry_get_attempt(&ctx);
    while (retry_should_retry(&ctx)) {
        uint8_t cur = retry_get_attempt(&ctx);
        PROP_ASSERT(cur >= prev);
        prev = cur;
    }
}

/* Property: double reset is idempotent */
PROPERTY(double_reset_idempotent, 5000) {
    retry_t ctx;
    retry_config_t cfg = {
        .strategy      = RETRY_FIXED,
        .base_delay_ms = 100,
        .max_delay_ms  = 100,
        .max_attempts  = 5,
    };
    retry_init(&ctx, &cfg);

    /* Use some attempts */
    retry_should_retry(&ctx);
    retry_should_retry(&ctx);

    /* Reset twice */
    retry_reset(&ctx);
    uint8_t after_first_reset = retry_get_attempt(&ctx);
    uint32_t delay_first = retry_get_delay(&ctx);

    retry_reset(&ctx);
    uint8_t after_second_reset = retry_get_attempt(&ctx);
    uint32_t delay_second = retry_get_delay(&ctx);

    PROP_ASSERT_EQ(after_first_reset, after_second_reset);
    PROP_ASSERT_EQ(delay_first, delay_second);
}

TEST_MAIN("retry properties") {
    RUN(max_retries_respected);
    RUN(reset_clears_state);
    RUN(delay_bounded);
    RUN(attempt_monotonic);
    RUN(double_reset_idempotent);
}
TEST_MAIN_END()
