/*
 * prop_log_rtt.c — property-based tests for log_rtt
 *
 * Requires: property_test.h from tooling/test/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/test prop_log_rtt.c ../log_rtt.c -o prop_log_rtt
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_rtt.h"
#include <string.h>

/* ---- mock RTT write ------------------------------------------------ */

#define CAP_SIZE 2048
static char g_captured[CAP_SIZE];
static size_t g_cap_len;
static unsigned g_last_channel;

static unsigned mock_rtt_write(void *ctx, unsigned channel,
                               const void *data, unsigned len) {
    (void)ctx;
    g_last_channel = channel;
    size_t to_copy = len < CAP_SIZE - g_cap_len ? len : CAP_SIZE - g_cap_len;
    memcpy(g_captured + g_cap_len, data, to_copy);
    g_cap_len += to_copy;
    return len;
}

static void reset_capture(void) {
    g_cap_len = 0;
    g_last_channel = 0;
    memset(g_captured, 0, CAP_SIZE);
}

/* Property: sink never crashes on arbitrary input */
PROPERTY(no_crash, 10000) {
    reset_capture();
    log_rtt_t r;
    unsigned ch = ARBI_UINT8(seed) % 16;
    log_rtt_init(&r, mock_rtt_write, NULL, ch);

    char buf[128];
    unsigned len = (unsigned)ARBI_RANGE(seed, 0, 127);
    ARBI_FILL(seed, (uint8_t *)buf, len);
    for (unsigned i = 0; i < len; i++) {
        if (buf[i] == '\0') buf[i] = 'x';
    }

    log_rtt_sink(&r, buf, len);
    PROP_ASSERT(1);
}

/* Property: data forwarded to RTT write matches input */
PROPERTY(data_forwarded, 10000) {
    reset_capture();
    log_rtt_t r;
    log_rtt_init(&r, mock_rtt_write, NULL, 0);

    char input[64];
    unsigned len = (unsigned)ARBI_RANGE(seed, 1, 64);
    ARBI_FILL(seed, (uint8_t *)input, len);
    for (unsigned i = 0; i < len; i++) {
        if (input[i] == '\0') input[i] = 'A';
    }

    log_rtt_sink(&r, input, len);
    PROP_ASSERT_EQ(g_cap_len, len);
    PROP_ASSERT_MEM_EQ(g_captured, input, len);
}

/* Property: channel is preserved correctly */
PROPERTY(channel_preserved, 5000) {
    reset_capture();
    unsigned ch = (unsigned)ARBI_RANGE(seed, 0, 15);
    log_rtt_t r;
    log_rtt_init(&r, mock_rtt_write, NULL, ch);

    log_rtt_sink(&r, "test", 4);
    PROP_ASSERT_EQ(g_last_channel, ch);
}

TEST_MAIN("log_rtt properties") {
    RUN(no_crash);
    RUN(data_forwarded);
    RUN(channel_preserved);
}
TEST_MAIN_END()
