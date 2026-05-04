/*
 * prop_log_color.c — property-based tests for log_color
 *
 * Requires: property_test.h from tooling/test/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/test prop_log_color.c ../log_color.c -o prop_log_color
 */

#include "test_harness.h"
#include "property_test.h"
#include "../log_color.h"
#include <string.h>

/* ---- capture output ------------------------------------------------ */

#define CAP_SIZE 2048
static char g_captured[CAP_SIZE];
static size_t g_cap_len;
static int g_call_count;

static void capture_fn(void *ctx, const char *line, size_t len) {
    (void)ctx;
    size_t to_copy = len < CAP_SIZE - g_cap_len ? len : CAP_SIZE - g_cap_len;
    memcpy(g_captured + g_cap_len, line, to_copy);
    g_cap_len += to_copy;
    g_call_count++;
}

static void reset_capture(void) {
    g_cap_len = 0;
    g_call_count = 0;
    memset(g_captured, 0, CAP_SIZE);
}

/* Property: sink never crashes on arbitrary input */
PROPERTY(no_crash, 10000) {
    reset_capture();
    log_color_t lc;
    log_color_init(&lc, capture_fn, NULL);

    char buf[128];
    unsigned len = (unsigned)ARBI_RANGE(seed, 0, 127);
    ARBI_FILL(seed, (uint8_t *)buf, len);
    for (unsigned i = 0; i < len; i++) {
        if (buf[i] == '\0') buf[i] = 'x';
    }

    log_color_sink(&lc, buf, len);
    PROP_ASSERT(1);
}

/* Property: output contains ANSI escape for known level lines */
PROPERTY(ansi_escape_present, 10000) {
    reset_capture();
    log_color_t lc;
    log_color_init(&lc, capture_fn, NULL);

    /* Build a log-format line with known level char */
    const char *levels = "EWID";
    char lvl = levels[ARBI_UINT8(seed) % 4];
    char line[64];
    int len = snprintf(line, sizeof(line), "[%c] [mod] message\n", lvl);

    log_color_sink(&lc, line, (size_t)len);

    /* Output should contain ESC char (0x1B) for ANSI color */
    PROP_ASSERT(g_cap_len > 0);
    int found_esc = 0;
    for (size_t i = 0; i < g_cap_len; i++) {
        if (g_captured[i] == '\033') {
            found_esc = 1;
            break;
        }
    }
    PROP_ASSERT(found_esc);
}

/* Property: output always forwarded (never swallowed) */
PROPERTY(always_forwarded, 5000) {
    reset_capture();
    log_color_t lc;
    log_color_init(&lc, capture_fn, NULL);

    char buf[32];
    unsigned len = (unsigned)ARBI_RANGE(seed, 1, 31);
    ARBI_FILL(seed, (uint8_t *)buf, len);
    for (unsigned i = 0; i < len; i++) {
        if (buf[i] == '\0') buf[i] = 'A';
    }

    log_color_sink(&lc, buf, len);
    PROP_ASSERT_EQ(g_call_count, 1);
    /* Output should be at least as long as input (color adds bytes) */
    PROP_ASSERT(g_cap_len >= len);
}

/* Property: output contains ANSI reset sequence */
PROPERTY(ansi_reset_present, 5000) {
    reset_capture();
    log_color_t lc;
    log_color_init(&lc, capture_fn, NULL);

    log_color_sink(&lc, "[E] err\n", 8);

    /* Should contain reset sequence \033[0m */
    g_captured[g_cap_len] = '\0';
    PROP_ASSERT(strstr(g_captured, "\033[0m") != NULL);
}

TEST_MAIN("log_color properties") {
    RUN(no_crash);
    RUN(ansi_escape_present);
    RUN(always_forwarded);
    RUN(ansi_reset_present);
}
TEST_MAIN_END()
