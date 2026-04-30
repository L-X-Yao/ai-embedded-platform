#include "test_harness.h"
#include "log_rate.h"

#include <string.h>

static int g_count;
static char g_last[256];

static void capture(void *ctx, const char *line, size_t len) {
    (void)ctx;
    g_count++;
    size_t n = len < sizeof(g_last) - 1 ? len : sizeof(g_last) - 1;
    memcpy(g_last, line, n);
    g_last[n] = '\0';
}

static uint32_t g_time_ms;
static uint32_t fake_time(void) { return g_time_ms; }

static void reset(void) {
    g_count = 0;
    g_last[0] = '\0';
    g_time_ms = 0;
}

TEST(test_within_limit) {
    reset();
    log_rate_t r;
    log_rate_init(&r, capture, NULL, fake_time, 3, 1000);

    log_rate_sink(&r, "A\n", 2);
    log_rate_sink(&r, "B\n", 2);
    log_rate_sink(&r, "C\n", 2);
    ASSERT_EQ(g_count, 3);
    ASSERT_EQ(log_rate_dropped(&r), 0);
}

TEST(test_exceeds_limit) {
    reset();
    log_rate_t r;
    log_rate_init(&r, capture, NULL, fake_time, 2, 1000);

    log_rate_sink(&r, "A\n", 2);
    log_rate_sink(&r, "B\n", 2);
    log_rate_sink(&r, "C\n", 2);
    log_rate_sink(&r, "D\n", 2);

    ASSERT_EQ(g_count, 2);
    ASSERT_EQ(log_rate_dropped(&r), 2);
}

TEST(test_refill_after_interval) {
    reset();
    log_rate_t r;
    log_rate_init(&r, capture, NULL, fake_time, 2, 1000);

    log_rate_sink(&r, "A\n", 2);
    log_rate_sink(&r, "B\n", 2);
    log_rate_sink(&r, "C\n", 2);
    ASSERT_EQ(g_count, 2);

    g_time_ms = 1000;
    log_rate_sink(&r, "D\n", 2);
    ASSERT_EQ(g_count, 3);
}

TEST(test_multiple_intervals) {
    reset();
    log_rate_t r;
    log_rate_init(&r, capture, NULL, fake_time, 1, 500);

    log_rate_sink(&r, "A\n", 2);
    log_rate_sink(&r, "B\n", 2);
    ASSERT_EQ(g_count, 1);

    g_time_ms = 500;
    log_rate_sink(&r, "C\n", 2);
    ASSERT_EQ(g_count, 2);

    g_time_ms = 1000;
    log_rate_sink(&r, "D\n", 2);
    ASSERT_EQ(g_count, 3);
}

TEST(test_reset) {
    reset();
    log_rate_t r;
    log_rate_init(&r, capture, NULL, fake_time, 1, 1000);

    log_rate_sink(&r, "A\n", 2);
    log_rate_sink(&r, "B\n", 2);
    ASSERT_EQ(log_rate_dropped(&r), 1);

    log_rate_reset(&r);
    ASSERT_EQ(log_rate_dropped(&r), 0);

    log_rate_sink(&r, "C\n", 2);
    ASSERT_EQ(g_count, 2);
}

TEST(test_null_safety) {
    log_rate_sink(NULL, "x", 1);
    log_rate_init(NULL, NULL, NULL, NULL, 0, 0);
    ASSERT_EQ(log_rate_dropped(NULL), 0);
    log_rate_reset(NULL);
}

TEST_MAIN("log_rate")
    RUN(test_within_limit);
    RUN(test_exceeds_limit);
    RUN(test_refill_after_interval);
    RUN(test_multiple_intervals);
    RUN(test_reset);
    RUN(test_null_safety);
TEST_MAIN_END()
