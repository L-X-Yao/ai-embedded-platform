#include "test_harness.h"
#include "log_dedup.h"

#include <string.h>

#define CAP_LINES 16
#define CAP_LEN   256

static char g_cap[CAP_LINES][CAP_LEN];
static int  g_cap_count;

static void capture(void *ctx, const char *line, size_t len) {
    (void)ctx;
    if (g_cap_count < CAP_LINES) {
        size_t n = len < CAP_LEN - 1 ? len : CAP_LEN - 1;
        memcpy(g_cap[g_cap_count], line, n);
        g_cap[g_cap_count][n] = '\0';
        g_cap_count++;
    }
}

static void reset(void) {
    g_cap_count = 0;
    memset(g_cap, 0, sizeof(g_cap));
}

TEST(test_no_duplicates) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    log_dedup_sink(&d, "[I] [a] msg1\n", 13);
    log_dedup_sink(&d, "[I] [a] msg2\n", 13);
    log_dedup_sink(&d, "[I] [a] msg3\n", 13);
    ASSERT_EQ(g_cap_count, 3);
    ASSERT_EQ(log_dedup_suppressed(&d), 0);
}

TEST(test_consecutive_duplicates) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    log_dedup_sink(&d, "[I] [a] same\n", 13);
    log_dedup_sink(&d, "[I] [a] same\n", 13);
    log_dedup_sink(&d, "[I] [a] same\n", 13);
    ASSERT_EQ(g_cap_count, 1);
    ASSERT_EQ(log_dedup_suppressed(&d), 2);
}

TEST(test_repeat_message_on_change) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    log_dedup_sink(&d, "[I] [a] same\n", 13);
    log_dedup_sink(&d, "[I] [a] same\n", 13);
    log_dedup_sink(&d, "[I] [a] same\n", 13);
    log_dedup_sink(&d, "[I] [a] diff\n", 13);

    ASSERT_EQ(g_cap_count, 3);
    ASSERT_TRUE(strstr(g_cap[0], "same") != NULL);
    ASSERT_TRUE(strstr(g_cap[1], "repeated 2 times") != NULL);
    ASSERT_TRUE(strstr(g_cap[2], "diff") != NULL);
}

TEST(test_flush_pending) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    log_dedup_sink(&d, "[E] [x] err\n", 12);
    log_dedup_sink(&d, "[E] [x] err\n", 12);
    ASSERT_EQ(g_cap_count, 1);

    log_dedup_flush(&d);
    ASSERT_EQ(g_cap_count, 2);
    ASSERT_TRUE(strstr(g_cap[1], "repeated 1 times") != NULL);
}

TEST(test_timestamp_ignored_for_comparison) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    log_dedup_sink(&d, "[00000100] [I] [a] msg\n", 23);
    log_dedup_sink(&d, "[00000200] [I] [a] msg\n", 23);
    log_dedup_sink(&d, "[00000300] [I] [a] msg\n", 23);

    ASSERT_EQ(g_cap_count, 1);
    ASSERT_EQ(log_dedup_suppressed(&d), 2);
}

TEST(test_no_timestamp_comparison) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    log_dedup_sink(&d, "[I] [a] x\n", 10);
    log_dedup_sink(&d, "[I] [a] x\n", 10);
    ASSERT_EQ(g_cap_count, 1);
    ASSERT_EQ(log_dedup_suppressed(&d), 1);
}

TEST(test_single_line_no_repeat) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    log_dedup_sink(&d, "[I] [a] once\n", 13);
    log_dedup_flush(&d);

    ASSERT_EQ(g_cap_count, 1);
    ASSERT_TRUE(strstr(g_cap[0], "once") != NULL);
}

TEST(test_null_safety) {
    log_dedup_sink(NULL, "x", 1);
    log_dedup_init(NULL, NULL, NULL);
    log_dedup_flush(NULL);
    ASSERT_EQ(log_dedup_suppressed(NULL), 0);
}

TEST_MAIN("log_dedup")
    RUN(test_no_duplicates);
    RUN(test_consecutive_duplicates);
    RUN(test_repeat_message_on_change);
    RUN(test_flush_pending);
    RUN(test_timestamp_ignored_for_comparison);
    RUN(test_no_timestamp_comparison);
    RUN(test_single_line_no_repeat);
    RUN(test_null_safety);
TEST_MAIN_END()
