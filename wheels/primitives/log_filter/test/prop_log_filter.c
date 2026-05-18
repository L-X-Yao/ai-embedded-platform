// SPDX-License-Identifier: MIT
/*
 * prop_log_filter.c — property-based tests for log_filter
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_log_filter.c ../log_filter.c -o prop_log_filter
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_filter.h"
#include <string.h>
#include <stdio.h>

/* --- test infrastructure ------------------------------------------------ */

static char g_captured[4096];
static size_t g_captured_len;
static int g_capture_count;

static void capture_fn(void *ctx, const char *line, size_t len)
{
    (void)ctx;
    if (g_captured_len + len < sizeof(g_captured)) {
        memcpy(g_captured + g_captured_len, line, len);
        g_captured_len += len;
    }
    g_capture_count++;
}

static void reset_capture(void)
{
    g_captured_len = 0;
    g_capture_count = 0;
    memset(g_captured, 0, sizeof(g_captured));
}

/* --- properties --------------------------------------------------------- */

/* Property: sink never crashes on random input */
PROPERTY(no_crash_random_input, 50000) {
    reset_capture();
    log_filter_t f;
    log_filter_mode_t mode = (ARBI_UINT8(seed) % 2 == 0) ?
                              LOG_FILTER_ALLOW : LOG_FILTER_DENY;
    log_filter_init(&f, capture_fn, NULL, mode);
    log_filter_add(&f, "uart");

    char line[128];
    unsigned len = (unsigned)ARBI_RANGE(seed, 0, 127);
    ARBI_FILL(seed, (uint8_t *)line, len);
    /* Replace NUL so it behaves like a string */
    for (unsigned i = 0; i < len; i++) {
        if (line[i] == '\0') line[i] = 'A';
    }

    log_filter_sink(&f, line, len);
    PROP_ASSERT(1);  /* survived */
}

/* Property: determinism — same input produces same forwarding decision */
PROPERTY(deterministic_forwarding, 5000) {
    char line[64];
    unsigned len = (unsigned)ARBI_RANGE(seed, 1, 63);
    uint32_t fill_seed = seed;
    ARBI_FILL(fill_seed, (uint8_t *)line, len);
    for (unsigned i = 0; i < len; i++) {
        if (line[i] == '\0') line[i] = 'A';
    }

    int counts[2];
    for (int pass = 0; pass < 2; pass++) {
        reset_capture();
        log_filter_t f;
        log_filter_init(&f, capture_fn, NULL, LOG_FILTER_ALLOW);
        log_filter_add(&f, "net");
        log_filter_sink(&f, line, len);
        counts[pass] = g_capture_count;
    }
    PROP_ASSERT_EQ(counts[0], counts[1]);
}

/* Property: ALLOW mode — listed module lines are forwarded */
PROPERTY(allow_mode_forwards_listed, 5000) {
    reset_capture();
    log_filter_t f;
    log_filter_init(&f, capture_fn, NULL, LOG_FILTER_ALLOW);

    /* Generate a random module name */
    char mod[8];
    unsigned mlen = (unsigned)ARBI_RANGE(seed, 1, 7);
    for (unsigned i = 0; i < mlen; i++) {
        mod[i] = (char)('a' + (ARBI_UINT8(seed) % 26));
    }
    mod[mlen] = '\0';
    log_filter_add(&f, mod);

    /* Build a log line: "[I] [modname] some text" */
    char line[128];
    int written = snprintf(line, sizeof(line), "[I] [%s] test message", mod);
    PROP_ASSERT(written > 0);

    log_filter_sink(&f, line, (size_t)written);
    PROP_ASSERT_EQ(g_capture_count, 1);
}

/* Property: DENY mode — listed module lines are blocked */
PROPERTY(deny_mode_blocks_listed, 5000) {
    reset_capture();
    log_filter_t f;
    log_filter_init(&f, capture_fn, NULL, LOG_FILTER_DENY);

    char mod[8];
    unsigned mlen = (unsigned)ARBI_RANGE(seed, 1, 7);
    for (unsigned i = 0; i < mlen; i++) {
        mod[i] = (char)('a' + (ARBI_UINT8(seed) % 26));
    }
    mod[mlen] = '\0';
    log_filter_add(&f, mod);

    char line[128];
    int written = snprintf(line, sizeof(line), "[D] [%s] debug noise", mod);
    PROP_ASSERT(written > 0);

    log_filter_sink(&f, line, (size_t)written);
    PROP_ASSERT_EQ(g_capture_count, 0);
}

/* Property: lines without module tag always pass through */
PROPERTY(no_tag_always_forwarded, 50000) {
    reset_capture();
    log_filter_t f;
    log_filter_mode_t mode = (ARBI_UINT8(seed) % 2 == 0) ?
                              LOG_FILTER_ALLOW : LOG_FILTER_DENY;
    log_filter_init(&f, capture_fn, NULL, mode);
    log_filter_add(&f, "xyz");

    /* Line with no brackets at all */
    const char *line = "plain text no brackets";
    log_filter_sink(&f, line, strlen(line));
    PROP_ASSERT_EQ(g_capture_count, 1);
}

/* Property: add returns -1 when full */
PROPERTY(add_overflow_returns_neg1, 5000) {
    log_filter_t f;
    log_filter_init(&f, capture_fn, NULL, LOG_FILTER_ALLOW);

    for (int i = 0; i < LOG_FILTER_MAX_MODULES; i++) {
        int rc = log_filter_add(&f, "mod");
        PROP_ASSERT_EQ(rc, 0);
    }
    int rc = log_filter_add(&f, "overflow");
    PROP_ASSERT_EQ(rc, -1);
}

TEST_MAIN("log_filter properties") {
    RUN(no_crash_random_input);
    RUN(deterministic_forwarding);
    RUN(allow_mode_forwards_listed);
    RUN(deny_mode_blocks_listed);
    RUN(no_tag_always_forwarded);
    RUN(add_overflow_returns_neg1);
}
TEST_MAIN_END()
