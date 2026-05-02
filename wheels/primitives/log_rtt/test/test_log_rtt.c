// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "log_rtt.h"

#include <string.h>

static char g_buf[256];
static unsigned g_len;
static unsigned g_channel;
static int g_calls;

static unsigned mock_rtt_write(void *ctx, unsigned channel,
                               const void *data, unsigned len) {
    (void)ctx;
    g_channel = channel;
    unsigned n = len < sizeof(g_buf) ? len : (unsigned)sizeof(g_buf);
    memcpy(g_buf, data, n);
    g_len = n;
    g_calls++;
    return n;
}

static void reset(void) {
    memset(g_buf, 0, sizeof(g_buf));
    g_len = 0;
    g_channel = 99;
    g_calls = 0;
}

TEST(test_basic_write) {
    reset();
    log_rtt_t r;
    log_rtt_init(&r, mock_rtt_write, NULL, 0);
    log_rtt_sink(&r, "[I] [app] boot\n", 15);
    ASSERT_EQ(g_calls, 1);
    ASSERT_EQ(g_len, 15);
    ASSERT_EQ(g_channel, 0);
    ASSERT_MEMEQ(g_buf, "[I] [app] boot\n");
}

TEST(test_channel_selection) {
    reset();
    log_rtt_t r;
    log_rtt_init(&r, mock_rtt_write, NULL, 2);
    log_rtt_sink(&r, "x\n", 2);
    ASSERT_EQ(g_channel, 2);
}

TEST(test_multiple_writes) {
    reset();
    log_rtt_t r;
    log_rtt_init(&r, mock_rtt_write, NULL, 0);
    log_rtt_sink(&r, "AAA\n", 4);
    log_rtt_sink(&r, "BBB\n", 4);
    ASSERT_EQ(g_calls, 2);
}

TEST(test_null_safety) {
    log_rtt_sink(NULL, "x", 1);
    log_rtt_init(NULL, NULL, NULL, 0);

    log_rtt_t r;
    log_rtt_init(&r, NULL, NULL, 0);
    log_rtt_sink(&r, "x", 1);

    log_rtt_init(&r, mock_rtt_write, NULL, 0);
    log_rtt_sink(&r, NULL, 0);
    log_rtt_sink(&r, "x", 0);
}

TEST_MAIN("log_rtt")
    RUN(test_basic_write);
    RUN(test_channel_selection);
    RUN(test_multiple_writes);
    RUN(test_null_safety);
TEST_MAIN_END()
