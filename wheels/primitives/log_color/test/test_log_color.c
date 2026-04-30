#include "test_harness.h"
#include "log_color.h"

#include <string.h>

#define CAP_LEN 512
static char g_cap[CAP_LEN];
static size_t g_cap_len;

static void capture(void *ctx, const char *line, size_t len) {
    (void)ctx;
    size_t n = len < CAP_LEN - 1 ? len : CAP_LEN - 1;
    memcpy(g_cap, line, n);
    g_cap[n] = '\0';
    g_cap_len = n;
}

static void reset(void) {
    memset(g_cap, 0, sizeof(g_cap));
    g_cap_len = 0;
}

TEST(test_error_red) {
    reset();
    log_color_t lc;
    log_color_init(&lc, capture, NULL);
    const char *line = "[E] [app] crash\n";
    log_color_sink(&lc, line, strlen(line));
    ASSERT_TRUE(g_cap_len > 0);
    ASSERT_TRUE(strstr(g_cap, "\033[31m") != NULL);
    ASSERT_TRUE(strstr(g_cap, "\033[0m") != NULL);
    ASSERT_TRUE(strstr(g_cap, "[E] [app] crash") != NULL);
}

TEST(test_warn_yellow) {
    reset();
    log_color_t lc;
    log_color_init(&lc, capture, NULL);
    const char *line = "[W] [drv] retry\n";
    log_color_sink(&lc, line, strlen(line));
    ASSERT_TRUE(strstr(g_cap, "\033[33m") != NULL);
}

TEST(test_info_green) {
    reset();
    log_color_t lc;
    log_color_init(&lc, capture, NULL);
    const char *line = "[00001234] [I] [net] connected\n";
    log_color_sink(&lc, line, strlen(line));
    ASSERT_TRUE(strstr(g_cap, "\033[32m") != NULL);
}

TEST(test_debug_gray) {
    reset();
    log_color_t lc;
    log_color_init(&lc, capture, NULL);
    const char *line = "[D] [test] detail\n";
    log_color_sink(&lc, line, strlen(line));
    ASSERT_TRUE(strstr(g_cap, "\033[90m") != NULL);
}

TEST(test_no_level_passthrough) {
    reset();
    log_color_t lc;
    log_color_init(&lc, capture, NULL);
    const char *line = "raw message\n";
    log_color_sink(&lc, line, strlen(line));
    ASSERT_MEMEQ(g_cap, "raw message\n");
}

TEST(test_with_timestamp) {
    reset();
    log_color_t lc;
    log_color_init(&lc, capture, NULL);
    const char *line = "[00005678] [E] [app] fatal\n";
    log_color_sink(&lc, line, strlen(line));
    ASSERT_TRUE(strstr(g_cap, "\033[31m") != NULL);
    ASSERT_TRUE(strstr(g_cap, "[00005678]") != NULL);
    ASSERT_TRUE(strstr(g_cap, "fatal") != NULL);
}

TEST(test_preserves_newline) {
    reset();
    log_color_t lc;
    log_color_init(&lc, capture, NULL);
    const char *line = "[I] [x] msg\n";
    log_color_sink(&lc, line, strlen(line));
    ASSERT_EQ(g_cap[g_cap_len - 1], '\n');
}

TEST(test_null_safety) {
    log_color_sink(NULL, "x", 1);

    log_color_t lc;
    log_color_init(&lc, capture, NULL);
    log_color_sink(&lc, NULL, 0);
    log_color_sink(&lc, "x", 0);

    log_color_init(NULL, NULL, NULL);
}

TEST_MAIN("log_color")
    RUN(test_error_red);
    RUN(test_warn_yellow);
    RUN(test_info_green);
    RUN(test_debug_gray);
    RUN(test_no_level_passthrough);
    RUN(test_with_timestamp);
    RUN(test_preserves_newline);
    RUN(test_null_safety);
TEST_MAIN_END()
