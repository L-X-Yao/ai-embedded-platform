// SPDX-License-Identifier: MIT
/* log.c built with LOG_USE_PRINTF_TINY=1: lines must come out as with libc. */
#include "test_harness.h"
#include "log.h"

#include <string.h>

#define LOG_MODULE "pump"

static char g_line[512];
static size_t g_len;

static void capture(void *ctx, const char *line, size_t len) {
    (void)ctx;
    g_len = len;
    if (len < sizeof(g_line)) {
        memcpy(g_line, line, len);
        g_line[len] = '\0';
    }
}

static uint32_t fixed_time(void) {
    return 100u;
}

TEST(test_line_format) {
    LOG_INFO("pressure %d %s", 42, "high");
    ASSERT_STR_EQ(g_line, "[I] [pump] pressure 42 high\n");

    LOG_WARN("0x%04x %-3d|", 0xABu, 7);
    ASSERT_STR_EQ(g_line, "[W] [pump] 0x00ab 7  |\n");
}

TEST(test_timestamp_is_zero_padded) {
    log_set_time_fn(fixed_time);
    LOG_ERROR("x");
    log_set_time_fn(NULL);
    ASSERT_STR_EQ(g_line, "[00000100] [E] [pump] x\n");
}

/* The one place the two formatters differ on purpose, and so the proof of
 * which one ran: libc gives 1.25, printf_tiny stops and copies the format. */
TEST(test_float_is_copied_literally) {
    log_emit(LOG_LEVEL_INFO, LOG_MODULE, "p=%.2f bar", 1.25);
    ASSERT_STR_EQ(g_line, "[I] [pump] p=%.2f bar\n");
}

/* LOG_LINE_BUFSIZE is 256: 254 characters and the newline, and the NUL. */
TEST(test_long_line_is_cut_like_libc) {
    char text[400];
    memset(text, 'a', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';

    LOG_INFO("%s", text);
    ASSERT_EQ(g_len, 255);
    ASSERT_MEMEQ(g_line, "[I] [pump] aaa");
    ASSERT_EQ(g_line[253], 'a');
    ASSERT_EQ(g_line[254], '\n');
}

TEST_MAIN("log_tiny")
    log_add_sink(capture, NULL);
    RUN(test_line_format);
    RUN(test_timestamp_is_zero_padded);
    RUN(test_float_is_copied_literally);
    RUN(test_long_line_is_cut_like_libc);
TEST_MAIN_END()
