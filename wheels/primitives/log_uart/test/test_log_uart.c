// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "log_uart.h"

#include <string.h>

static uint8_t g_buf[256];
static size_t g_len;
static int g_calls;

static void mock_write(void *ctx, const uint8_t *data, size_t len) {
    (void)ctx;
    size_t n = len < sizeof(g_buf) ? len : sizeof(g_buf);
    memcpy(g_buf, data, n);
    g_len = n;
    g_calls++;
}

static int g_ctx_flag;
static void ctx_write(void *ctx, const uint8_t *data, size_t len) {
    (void)data; (void)len;
    int *p = (int *)ctx;
    *p = 42;
}

static void reset(void) {
    memset(g_buf, 0, sizeof(g_buf));
    g_len = 0;
    g_calls = 0;
}

TEST(test_basic_write) {
    reset();
    log_uart_t u;
    log_uart_init(&u, mock_write, NULL);
    log_uart_sink(&u, "[I] [app] boot\n", 15);
    ASSERT_EQ(g_calls, 1);
    ASSERT_EQ(g_len, 15);
    ASSERT_MEMEQ(g_buf, "[I] [app] boot\n");
}

TEST(test_multiple_writes) {
    reset();
    log_uart_t u;
    log_uart_init(&u, mock_write, NULL);
    log_uart_sink(&u, "AAA\n", 4);
    log_uart_sink(&u, "BBB\n", 4);
    ASSERT_EQ(g_calls, 2);
    ASSERT_MEMEQ(g_buf, "BBB\n");
}

TEST(test_ctx_passthrough) {
    g_ctx_flag = 0;
    log_uart_t u;
    log_uart_init(&u, ctx_write, &g_ctx_flag);
    log_uart_sink(&u, "x\n", 2);
    ASSERT_EQ(g_ctx_flag, 42);
}

TEST(test_null_safety) {
    log_uart_sink(NULL, "x", 1);
    log_uart_init(NULL, NULL, NULL);

    log_uart_t u;
    log_uart_init(&u, NULL, NULL);
    log_uart_sink(&u, "x", 1);

    log_uart_init(&u, mock_write, NULL);
    log_uart_sink(&u, NULL, 0);
    log_uart_sink(&u, "x", 0);
}

TEST_MAIN("log_uart")
    RUN(test_basic_write);
    RUN(test_multiple_writes);
    RUN(test_ctx_passthrough);
    RUN(test_null_safety);
TEST_MAIN_END()
