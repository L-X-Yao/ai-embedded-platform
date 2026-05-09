/*
 * prop_timeout.c — property-based tests for timeout
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_timeout.c ../timeout.c -o prop_timeout
 */

#include "test_harness.h"
#include "property_test.h"
#include "../timeout.h"

/* Property: not expired before duration elapses */
PROPERTY(not_expired_before, 10000) {
    timeout_t t;
    uint32_t start = ARBI_UINT32(seed);
    uint32_t duration = (uint32_t)ARBI_RANGE(seed, 2, 10000);
    timeout_set(&t, start, duration);

    /* Check at a point strictly before expiry */
    uint32_t offset = (uint32_t)ARBI_RANGE(seed, 0, (int)(duration - 2));
    uint32_t now = start + offset;
    PROP_ASSERT(!timeout_expired(&t, now));
}

/* Property: expired after duration elapses */
PROPERTY(expired_after, 10000) {
    timeout_t t;
    uint32_t start = ARBI_UINT32(seed);
    uint32_t duration = (uint32_t)ARBI_RANGE(seed, 1, 10000);
    timeout_set(&t, start, duration);

    uint32_t extra = (uint32_t)ARBI_RANGE(seed, 0, 5000);
    uint32_t now = start + duration + extra;
    PROP_ASSERT(timeout_expired(&t, now));
}

/* Property: remaining + elapsed == now - start (before expiry) */
PROPERTY(remaining_plus_elapsed, 10000) {
    timeout_t t;
    uint32_t start = ARBI_UINT32(seed);
    uint32_t duration = (uint32_t)ARBI_RANGE(seed, 10, 10000);
    timeout_set(&t, start, duration);

    uint32_t offset = (uint32_t)ARBI_RANGE(seed, 0, (int)(duration - 1));
    uint32_t now = start + offset;

    uint32_t rem = timeout_remaining(&t, now);
    uint32_t el = timeout_elapsed(&t, now);

    /* remaining + elapsed should equal duration */
    PROP_ASSERT_EQ(rem + el, duration);
}

/* Property: restart resets the timeout from current time */
PROPERTY(restart_resets, 5000) {
    timeout_t t;
    uint32_t start = ARBI_UINT32(seed);
    uint32_t duration = (uint32_t)ARBI_RANGE(seed, 10, 1000);
    timeout_set(&t, start, duration);

    /* Advance past expiry */
    uint32_t now = start + duration + 100;
    PROP_ASSERT(timeout_expired(&t, now));

    /* Restart */
    timeout_restart(&t, now);

    /* Should not be expired right after restart */
    PROP_ASSERT(!timeout_expired(&t, now));

    /* Should expire after duration again */
    PROP_ASSERT(timeout_expired(&t, now + duration));
}

/* Property: handles 32-bit wraparound correctly */
PROPERTY(wraparound_safe, 10000) {
    timeout_t t;
    /* Start near UINT32_MAX to force wraparound */
    uint32_t start = UINT32_MAX - (uint32_t)ARBI_RANGE(seed, 0, 1000);
    uint32_t duration = (uint32_t)ARBI_RANGE(seed, 1, 5000);
    timeout_set(&t, start, duration);

    /* Before expiry (may wrap) */
    uint32_t before = start + duration / 2;
    PROP_ASSERT(!timeout_expired(&t, before));

    /* After expiry (wraps around) */
    uint32_t after = start + duration + 1;
    PROP_ASSERT(timeout_expired(&t, after));
}

TEST_MAIN("timeout properties") {
    RUN(not_expired_before);
    RUN(expired_after);
    RUN(remaining_plus_elapsed);
    RUN(restart_resets);
    RUN(wraparound_safe);
}
TEST_MAIN_END()
