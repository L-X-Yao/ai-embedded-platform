// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "log_filter.h"

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

TEST(test_allow_mode_match) {
    reset();
    log_filter_t f;
    log_filter_init(&f, capture, NULL, LOG_FILTER_ALLOW);
    log_filter_add(&f, "net");

    log_filter_sink(&f, "[I] [net] connected\n", 20);
    log_filter_sink(&f, "[I] [app] boot\n", 15);

    ASSERT_EQ(g_cap_count, 1);
    ASSERT_TRUE(strstr(g_cap[0], "[net]") != NULL);
}

TEST(test_allow_mode_multiple) {
    reset();
    log_filter_t f;
    log_filter_init(&f, capture, NULL, LOG_FILTER_ALLOW);
    log_filter_add(&f, "net");
    log_filter_add(&f, "ble");

    log_filter_sink(&f, "[I] [net] ok\n", 13);
    log_filter_sink(&f, "[I] [ble] ok\n", 13);
    log_filter_sink(&f, "[I] [drv] ok\n", 13);

    ASSERT_EQ(g_cap_count, 2);
}

TEST(test_deny_mode) {
    reset();
    log_filter_t f;
    log_filter_init(&f, capture, NULL, LOG_FILTER_DENY);
    log_filter_add(&f, "noisy");

    log_filter_sink(&f, "[D] [noisy] spam\n", 17);
    log_filter_sink(&f, "[I] [app] ok\n", 13);
    log_filter_sink(&f, "[E] [drv] err\n", 14);

    ASSERT_EQ(g_cap_count, 2);
    ASSERT_TRUE(strstr(g_cap[0], "[app]") != NULL);
    ASSERT_TRUE(strstr(g_cap[1], "[drv]") != NULL);
}

TEST(test_no_module_passthrough) {
    reset();
    log_filter_t f;
    log_filter_init(&f, capture, NULL, LOG_FILTER_ALLOW);
    log_filter_add(&f, "app");

    log_filter_sink(&f, "raw line no brackets\n", 21);
    ASSERT_EQ(g_cap_count, 1);
}

TEST(test_with_timestamp) {
    reset();
    log_filter_t f;
    log_filter_init(&f, capture, NULL, LOG_FILTER_ALLOW);
    log_filter_add(&f, "imu");

    log_filter_sink(&f, "[00012345] [I] [imu] data ready\n", 31);
    log_filter_sink(&f, "[00012346] [I] [gps] fix\n", 25);

    ASSERT_EQ(g_cap_count, 1);
    ASSERT_TRUE(strstr(g_cap[0], "[imu]") != NULL);
}

TEST(test_clear) {
    log_filter_t f;
    log_filter_init(&f, capture, NULL, LOG_FILTER_ALLOW);
    log_filter_add(&f, "x");

    reset();
    log_filter_sink(&f, "[I] [x] msg\n", 12);
    ASSERT_EQ(g_cap_count, 1);

    log_filter_clear(&f);
    reset();
    log_filter_sink(&f, "[I] [x] msg\n", 12);
    ASSERT_EQ(g_cap_count, 0);
}

TEST(test_slots_exhausted) {
    log_filter_t f;
    log_filter_init(&f, capture, NULL, LOG_FILTER_ALLOW);
    for (int i = 0; i < LOG_FILTER_MAX_MODULES; i++) {
        ASSERT_EQ(log_filter_add(&f, "m"), 0);
    }
    ASSERT_EQ(log_filter_add(&f, "overflow"), -1);
}

TEST(test_null_safety) {
    log_filter_sink(NULL, "x", 1);
    log_filter_init(NULL, NULL, NULL, LOG_FILTER_ALLOW);
    log_filter_add(NULL, "x");
    log_filter_clear(NULL);

    log_filter_t f;
    log_filter_init(&f, capture, NULL, LOG_FILTER_ALLOW);
    log_filter_add(&f, NULL);
    log_filter_sink(&f, NULL, 0);
}

TEST_MAIN("log_filter")
    RUN(test_allow_mode_match);
    RUN(test_allow_mode_multiple);
    RUN(test_deny_mode);
    RUN(test_no_module_passthrough);
    RUN(test_with_timestamp);
    RUN(test_clear);
    RUN(test_slots_exhausted);
    RUN(test_null_safety);
TEST_MAIN_END()
