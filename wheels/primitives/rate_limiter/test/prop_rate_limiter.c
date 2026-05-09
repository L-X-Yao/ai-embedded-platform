/*
 * prop_rate_limiter.c — property-based tests for rate_limiter
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_rate_limiter.c ../rate_limiter.c -o prop_rate_limiter
 */

#include "test_harness.h"
#include "property_test.h"
#include "../rate_limiter.h"

/* Property: allows up to max_tokens at tick 0 (bucket starts full) */
PROPERTY(allows_up_to_limit, 10000) {
    rate_limiter_t rl;
    uint32_t max_tokens = (uint32_t)ARBI_RANGE(seed, 1, 100);
    uint32_t rate = (uint32_t)ARBI_RANGE(seed, 1, 10);
    rate_limiter_err_t err = rate_limiter_init(&rl, max_tokens, rate);
    PROP_ASSERT_EQ(err, RATE_LIMITER_OK);

    /* At tick 0, all max_tokens should be allowed */
    for (uint32_t i = 0; i < max_tokens; i++) {
        err = rate_limiter_try(&rl, 0);
        PROP_ASSERT_EQ(err, RATE_LIMITER_OK);
    }
}

/* Property: blocks beyond limit at same tick */
PROPERTY(blocks_beyond_limit, 10000) {
    rate_limiter_t rl;
    uint32_t max_tokens = (uint32_t)ARBI_RANGE(seed, 1, 50);
    rate_limiter_init(&rl, max_tokens, 1);

    /* Drain all tokens at tick 0 */
    for (uint32_t i = 0; i < max_tokens; i++) {
        rate_limiter_try(&rl, 0);
    }

    /* Next attempt should be denied */
    rate_limiter_err_t err = rate_limiter_try(&rl, 0);
    PROP_ASSERT_EQ(err, RATE_LIMITER_DENIED);
}

/* Property: refill allows new tokens after ticks pass */
PROPERTY(refill_allows_new, 5000) {
    rate_limiter_t rl;
    uint32_t max_tokens = (uint32_t)ARBI_RANGE(seed, 1, 20);
    uint32_t rate = (uint32_t)ARBI_RANGE(seed, 1, 5);
    rate_limiter_init(&rl, max_tokens, rate);

    /* Drain all tokens at tick 0 */
    for (uint32_t i = 0; i < max_tokens; i++) {
        rate_limiter_try(&rl, 0);
    }
    PROP_ASSERT_EQ(rate_limiter_try(&rl, 0), RATE_LIMITER_DENIED);

    /* Advance enough ticks to refill at least 1 token */
    uint32_t advance = 1;
    rate_limiter_err_t err = rate_limiter_try(&rl, advance);
    PROP_ASSERT_EQ(err, RATE_LIMITER_OK);
}

/* Property: allowed + denied counts match total attempts */
PROPERTY(count_consistency, 10000) {
    rate_limiter_t rl;
    uint32_t max_tokens = (uint32_t)ARBI_RANGE(seed, 1, 20);
    rate_limiter_init(&rl, max_tokens, 1);

    uint32_t attempts = (uint32_t)ARBI_RANGE(seed, 1, 50);
    for (uint32_t i = 0; i < attempts; i++) {
        uint32_t tick = i / 5;  /* slow refill */
        rate_limiter_try(&rl, tick);
    }

    PROP_ASSERT_EQ(rate_limiter_allowed(&rl) + rate_limiter_denied(&rl), attempts);
}

/* Property: reset restores full bucket */
PROPERTY(reset_restores_full, 5000) {
    rate_limiter_t rl;
    uint32_t max_tokens = (uint32_t)ARBI_RANGE(seed, 1, 20);
    rate_limiter_init(&rl, max_tokens, 1);

    /* Drain some tokens */
    uint32_t drain = (uint32_t)ARBI_RANGE(seed, 1, max_tokens);
    for (uint32_t i = 0; i < drain; i++) {
        rate_limiter_try(&rl, 0);
    }

    rate_limiter_reset(&rl);
    PROP_ASSERT_EQ(rate_limiter_available(&rl), max_tokens);
    PROP_ASSERT_EQ(rate_limiter_allowed(&rl), 0);
    PROP_ASSERT_EQ(rate_limiter_denied(&rl), 0);
}

TEST_MAIN("rate_limiter properties") {
    RUN(allows_up_to_limit);
    RUN(blocks_beyond_limit);
    RUN(refill_allows_new);
    RUN(count_consistency);
    RUN(reset_restores_full);
}
TEST_MAIN_END()
