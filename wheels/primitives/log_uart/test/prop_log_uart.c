// SPDX-License-Identifier: MIT
/*
 * prop_log_uart.c — property-based tests for log_uart
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_log_uart.c ../log_uart.c -o prop_log_uart
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_uart.h"
#include <string.h>

/* ---- mock UART write ----------------------------------------------- */

#define CAP_SIZE 2048
static uint8_t g_captured[CAP_SIZE];
static size_t g_cap_len;

static void mock_uart_write(void *ctx, const uint8_t *data, size_t len) {
    (void)ctx;
    size_t to_copy = len < CAP_SIZE - g_cap_len ? len : CAP_SIZE - g_cap_len;
    memcpy(g_captured + g_cap_len, data, to_copy);
    g_cap_len += to_copy;
}

static void reset_capture(void) {
    g_cap_len = 0;
    memset(g_captured, 0, CAP_SIZE);
}

/* Property: sink never crashes on arbitrary input */
PROPERTY(no_crash, 10000) {
    reset_capture();
    log_uart_t u;
    log_uart_init(&u, mock_uart_write, NULL);

    char buf[128];
    unsigned len = (unsigned)ARBI_RANGE(seed, 0, 127);
    ARBI_FILL(seed, (uint8_t *)buf, len);
    for (unsigned i = 0; i < len; i++) {
        if (buf[i] == '\0') buf[i] = 'x';
    }

    log_uart_sink(&u, buf, len);
    PROP_ASSERT(1);
}

/* Property: data forwarded to UART write matches input */
PROPERTY(data_forwarded, 10000) {
    reset_capture();
    log_uart_t u;
    log_uart_init(&u, mock_uart_write, NULL);

    char input[64];
    unsigned len = (unsigned)ARBI_RANGE(seed, 1, 64);
    ARBI_FILL(seed, (uint8_t *)input, len);
    for (unsigned i = 0; i < len; i++) {
        if (input[i] == '\0') input[i] = 'A';
    }

    log_uart_sink(&u, input, len);
    PROP_ASSERT_EQ(g_cap_len, len);
    PROP_ASSERT_MEM_EQ(g_captured, input, len);
}

/* Property: multiple writes accumulate in order */
PROPERTY(multi_write_order, 5000) {
    reset_capture();
    log_uart_t u;
    log_uart_init(&u, mock_uart_write, NULL);

    char bufs[4][16];
    size_t lens[4];
    unsigned count = (unsigned)ARBI_RANGE(seed, 1, 4);
    size_t total = 0;

    for (unsigned i = 0; i < count; i++) {
        lens[i] = (size_t)ARBI_RANGE(seed, 1, 16);
        ARBI_FILL(seed, (uint8_t *)bufs[i], lens[i]);
        for (size_t j = 0; j < lens[i]; j++) {
            if (bufs[i][j] == '\0') bufs[i][j] = 'A';
        }
        log_uart_sink(&u, bufs[i], lens[i]);
        total += lens[i];
    }

    PROP_ASSERT_EQ(g_cap_len, total);
    /* Verify order */
    size_t offset = 0;
    for (unsigned i = 0; i < count; i++) {
        PROP_ASSERT_MEM_EQ(g_captured + offset, bufs[i], lens[i]);
        offset += lens[i];
    }
}

TEST_MAIN("log_uart properties") {
    RUN(no_crash);
    RUN(data_forwarded);
    RUN(multi_write_order);
}
TEST_MAIN_END()
