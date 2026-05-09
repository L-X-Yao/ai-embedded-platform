/*
 * prop_log_rate.c — property-based tests for log_rate
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_log_rate.c ../log_rate.c -o prop_log_rate
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_rate.h"
#include <string.h>

/* --- test infrastructure ------------------------------------------------ */

static int g_forwarded;
static uint32_t g_mock_time;

static void capture_fn(void *ctx, const char *line, size_t len)
{
    (void)ctx; (void)line; (void)len;
    g_forwarded++;
}

static uint32_t mock_time_fn(void)
{
    return g_mock_time;
}

static void reset_state(void)
{
    g_forwarded = 0;
    g_mock_time = 0;
}

/* --- properties --------------------------------------------------------- */

/* Property: sink never crashes on random input */
PROPERTY(no_crash_random_input, 50000) {
    reset_state();
    log_rate_t r;
    uint32_t max_tokens = ARBI_RANGE(seed, 0, 100);
    uint32_t interval = ARBI_RANGE(seed, 1, 10000);
    log_rate_init(&r, capture_fn, NULL, mock_time_fn, max_tokens, interval);

    char line[64];
    unsigned len = (unsigned)ARBI_RANGE(seed, 0, 63);
    ARBI_FILL(seed, (uint8_t *)line, len);
    for (unsigned i = 0; i < len; i++) {
        if (line[i] == '\0') line[i] = 'A';
    }

    g_mock_time = ARBI_UINT32(seed);
    log_rate_sink(&r, line, len);
    PROP_ASSERT(1);  /* survived */
}

/* Property: determinism — same inputs produce same forward/drop counts */
PROPERTY(deterministic_counts, 5000) {
    int fwd[2];
    uint32_t drops[2];

    for (int pass = 0; pass < 2; pass++) {
        reset_state();
        log_rate_t r;
        log_rate_init(&r, capture_fn, NULL, mock_time_fn, 5, 1000);

        uint32_t local_seed = seed;
        unsigned ops = (unsigned)ARBI_RANGE(local_seed, 1, 30);
        for (unsigned i = 0; i < ops; i++) {
            g_mock_time = (uint32_t)(i * 100);
            log_rate_sink(&r, "msg", 3);
        }
        fwd[pass] = g_forwarded;
        drops[pass] = log_rate_dropped(&r);
    }
    PROP_ASSERT_EQ(fwd[0], fwd[1]);
    PROP_ASSERT_EQ(drops[0], drops[1]);
}

/* Property: forwarded + dropped == total calls */
PROPERTY(forwarded_plus_dropped_eq_calls, 50000) {
    reset_state();
    log_rate_t r;
    uint32_t max_tok = (uint32_t)ARBI_RANGE(seed, 1, 20);
    uint32_t interval = (uint32_t)ARBI_RANGE(seed, 100, 5000);
    log_rate_init(&r, capture_fn, NULL, mock_time_fn, max_tok, interval);

    unsigned calls = (unsigned)ARBI_RANGE(seed, 1, 50);
    for (unsigned i = 0; i < calls; i++) {
        g_mock_time = (uint32_t)(i * 50);
        log_rate_sink(&r, "x", 1);
    }

    PROP_ASSERT_EQ((uint32_t)g_forwarded + log_rate_dropped(&r), calls);
}

/* Property: within a single interval, at most max_tokens are forwarded */
PROPERTY(respects_max_tokens, 50000) {
    reset_state();
    uint32_t max_tok = (uint32_t)ARBI_RANGE(seed, 1, 20);
    log_rate_t r;
    log_rate_init(&r, capture_fn, NULL, mock_time_fn, max_tok, 10000);

    /* All calls at time 0 — no refill */
    g_mock_time = 0;
    unsigned calls = (unsigned)ARBI_RANGE(seed, 1, 100);
    for (unsigned i = 0; i < calls; i++) {
        log_rate_sink(&r, "m", 1);
    }

    PROP_ASSERT((uint32_t)g_forwarded <= max_tok);
}

/* Property: reset clears dropped counter and restores tokens */
PROPERTY(reset_restores_tokens, 5000) {
    reset_state();
    log_rate_t r;
    log_rate_init(&r, capture_fn, NULL, mock_time_fn, 2, 10000);

    /* Exhaust tokens */
    g_mock_time = 0;
    for (int i = 0; i < 10; i++) {
        log_rate_sink(&r, "x", 1);
    }
    PROP_ASSERT(log_rate_dropped(&r) > 0);

    log_rate_reset(&r);
    PROP_ASSERT_EQ(log_rate_dropped(&r), 0);

    /* After reset, should forward again */
    g_forwarded = 0;
    log_rate_sink(&r, "y", 1);
    PROP_ASSERT_EQ(g_forwarded, 1);
}

/* Property: time advance refills tokens */
PROPERTY(time_advance_refills, 5000) {
    reset_state();
    uint32_t max_tok = (uint32_t)ARBI_RANGE(seed, 1, 10);
    uint32_t interval = (uint32_t)ARBI_RANGE(seed, 100, 1000);
    log_rate_t r;
    log_rate_init(&r, capture_fn, NULL, mock_time_fn, max_tok, interval);

    /* Exhaust all tokens at time 0 */
    g_mock_time = 0;
    for (uint32_t i = 0; i < max_tok + 5; i++) {
        log_rate_sink(&r, "a", 1);
    }
    uint32_t dropped_before = log_rate_dropped(&r);
    PROP_ASSERT(dropped_before > 0);

    /* Advance time past one interval */
    g_mock_time = interval + 1;
    g_forwarded = 0;
    log_rate_sink(&r, "b", 1);
    PROP_ASSERT_EQ(g_forwarded, 1);  /* refilled, so forwarded */
}

TEST_MAIN("log_rate properties") {
    RUN(no_crash_random_input);
    RUN(deterministic_counts);
    RUN(forwarded_plus_dropped_eq_calls);
    RUN(respects_max_tokens);
    RUN(reset_restores_tokens);
    RUN(time_advance_refills);
}
TEST_MAIN_END()
