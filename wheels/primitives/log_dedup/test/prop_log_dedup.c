// SPDX-License-Identifier: MIT
/*
 * prop_log_dedup.c — property-based tests for log_dedup
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_log_dedup.c ../log_dedup.c -o prop_log_dedup
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_dedup.h"
#include <string.h>

/* ---- capture output ------------------------------------------------ */

#define CAP_SIZE 4096
static char g_captured[CAP_SIZE];
static size_t g_cap_len;
static int g_call_count;

static void capture_fn(void *ctx, const char *line, size_t len) {
    (void)ctx;
    size_t to_copy = len < CAP_SIZE - g_cap_len ? len : CAP_SIZE - g_cap_len;
    memcpy(g_captured + g_cap_len, line, to_copy);
    g_cap_len += to_copy;
    g_call_count++;
}

static void reset_capture(void) {
    g_cap_len = 0;
    g_call_count = 0;
    memset(g_captured, 0, CAP_SIZE);
}

/* Property: sink never crashes on arbitrary input */
PROPERTY(no_crash, 10000) {
    reset_capture();
    log_dedup_t d;
    log_dedup_init(&d, capture_fn, NULL);

    unsigned ops = (unsigned)ARBI_RANGE(seed, 1, 20);
    for (unsigned i = 0; i < ops; i++) {
        char buf[64];
        unsigned len = (unsigned)ARBI_RANGE(seed, 0, 63);
        ARBI_FILL(seed, (uint8_t *)buf, len);
        /* Ensure no embedded NUL that could confuse string ops */
        for (unsigned j = 0; j < len; j++) {
            if (buf[j] == '\0') buf[j] = 'A';
        }
        log_dedup_sink(&d, buf, len);
    }
    log_dedup_flush(&d);
    PROP_ASSERT(1);
}

/* Property: consecutive identical lines are suppressed */
PROPERTY(dedup_identical, 5000) {
    reset_capture();
    log_dedup_t d;
    log_dedup_init(&d, capture_fn, NULL);

    unsigned repeats = (unsigned)ARBI_RANGE(seed, 2, 50);
    for (unsigned i = 0; i < repeats; i++) {
        log_dedup_sink(&d, "same line", 9);
    }
    log_dedup_flush(&d);

    /* Only 1 original + 1 "repeated" message should have been forwarded */
    PROP_ASSERT(g_call_count <= 2);
    PROP_ASSERT(log_dedup_suppressed(&d) == repeats - 1);
}

/* Property: distinct lines always forwarded */
PROPERTY(distinct_forwarded, 5000) {
    reset_capture();
    log_dedup_t d;
    log_dedup_init(&d, capture_fn, NULL);

    unsigned count = (unsigned)ARBI_RANGE(seed, 1, 20);
    for (unsigned i = 0; i < count; i++) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "line-%u-%u", i, ARBI_UINT32(seed));
        log_dedup_sink(&d, buf, (size_t)len);
    }
    log_dedup_flush(&d);

    /* Each distinct line should be forwarded exactly once */
    PROP_ASSERT_EQ(g_call_count, (int)count);
    PROP_ASSERT_EQ(log_dedup_suppressed(&d), 0);
}

/* Property: suppressed count equals repeats minus first occurrence */
PROPERTY(suppressed_count_accurate, 5000) {
    reset_capture();
    log_dedup_t d;
    log_dedup_init(&d, capture_fn, NULL);

    unsigned r1 = (unsigned)ARBI_RANGE(seed, 1, 30);
    unsigned r2 = (unsigned)ARBI_RANGE(seed, 1, 30);

    for (unsigned i = 0; i < r1; i++)
        log_dedup_sink(&d, "aaa", 3);
    for (unsigned i = 0; i < r2; i++)
        log_dedup_sink(&d, "bbb", 3);
    log_dedup_flush(&d);

    uint32_t expected = (r1 > 1 ? r1 - 1 : 0) + (r2 > 1 ? r2 - 1 : 0);
    PROP_ASSERT_EQ(log_dedup_suppressed(&d), expected);
}

TEST_MAIN("log_dedup properties") {
    RUN(no_crash);
    RUN(dedup_identical);
    RUN(distinct_forwarded);
    RUN(suppressed_count_accurate);
}
TEST_MAIN_END()
