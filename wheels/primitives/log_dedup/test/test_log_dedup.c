// SPDX-License-Identifier: MIT
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
    ASSERT_TRUE(strstr(g_cap[1], "repeated 1 time]") != NULL);
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

static void feed(log_dedup_t *d, const char *line) {
    log_dedup_sink(d, line, strlen(line));
}

/* log.c writes no timestamp by default, so the first bracket is the level:
 * an ERROR must not be counted as a repeat of the WARN before it. */
TEST(test_level_is_not_a_timestamp) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    feed(&d, "[W] [pump] pressure high\n");
    feed(&d, "[E] [pump] pressure high\n");
    feed(&d, "[I] [pump] pressure high\n");

    ASSERT_EQ(g_cap_count, 3);
    ASSERT_EQ(log_dedup_suppressed(&d), 0);
    ASSERT_TRUE(strstr(g_cap[1], "[E]") != NULL);
    ASSERT_TRUE(strstr(g_cap[2], "[I]") != NULL);
}

TEST(test_level_compared_after_timestamp) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    feed(&d, "[00000100] [W] [pump] pressure high\n");
    feed(&d, "[00000200] [E] [pump] pressure high\n");
    feed(&d, "[00000300] [E] [pump] pressure high\n");

    ASSERT_EQ(g_cap_count, 2);
    ASSERT_EQ(log_dedup_suppressed(&d), 1);
}

/* Only digits make a timestamp: mixed or empty brackets stay in the text. */
TEST(test_non_digit_bracket_is_kept) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    feed(&d, "[12a] body\n");
    feed(&d, "[12b] body\n");
    feed(&d, "[] body\n");
    feed(&d, "[ ] body\n");

    ASSERT_EQ(g_cap_count, 4);
    ASSERT_EQ(log_dedup_suppressed(&d), 0);
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

TEST(test_deinit_flushes_and_clears) {
    reset();
    log_dedup_t d;
    log_dedup_init(&d, capture, NULL);

    /* send a line twice — second is suppressed */
    log_dedup_sink(&d, "[I] [a] msg\n", 12);
    log_dedup_sink(&d, "[I] [a] msg\n", 12);
    ASSERT_EQ(g_cap_count, 1);

    /* deinit must flush the pending repeat before clearing state */
    log_dedup_deinit(&d);

    /* flush emitted the "[repeated 1 time]" message */
    ASSERT_EQ(g_cap_count, 2);
    ASSERT_TRUE(strstr(g_cap[1], "repeated 1 time") != NULL);

    /* inner_fn must be NULL after deinit */
    ASSERT_TRUE(d.inner_fn == NULL);
}

TEST(test_deinit_null_is_safe) {
    log_dedup_deinit(NULL);
    ASSERT_TRUE(1);
}

TEST_MAIN("log_dedup")
    RUN(test_no_duplicates);
    RUN(test_consecutive_duplicates);
    RUN(test_repeat_message_on_change);
    RUN(test_flush_pending);
    RUN(test_timestamp_ignored_for_comparison);
    RUN(test_no_timestamp_comparison);
    RUN(test_level_is_not_a_timestamp);
    RUN(test_level_compared_after_timestamp);
    RUN(test_non_digit_bracket_is_kept);
    RUN(test_single_line_no_repeat);
    RUN(test_null_safety);
    RUN(test_deinit_flushes_and_clears);
    RUN(test_deinit_null_is_safe);
TEST_MAIN_END()
