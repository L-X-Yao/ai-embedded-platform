// SPDX-License-Identifier: MIT
/*
 * prop_log.c — property-based tests for log
 *
 * Requires: property_test.h from tooling/harness/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_log.c ../log.c -o prop_log
 */

#define LOG_MODULE "prop"
#include "test_harness.h"
#include "property_test.h"
#include "../log.h"
#include <string.h>

/* ---- capture sink -------------------------------------------------- */

#define CAP_SIZE 1024
static char g_captured[CAP_SIZE];
static size_t g_cap_len;

static void capture_sink(void *ctx, const char *line, size_t len) {
    (void)ctx;
    size_t to_copy = len < CAP_SIZE - g_cap_len ? len : CAP_SIZE - g_cap_len;
    memcpy(g_captured + g_cap_len, line, to_copy);
    g_cap_len += to_copy;
}

static void reset_log_state(void) {
    /* Remove all sinks — log has global state, must clear between iterations */
    for (int i = 0; i < 4; i++) {
        log_remove_sink(i);
    }
    g_cap_len = 0;
    memset(g_captured, 0, CAP_SIZE);
    log_set_level(LOG_LEVEL_DEBUG);
    log_set_time_fn(NULL);
}

/* Property: log_emit never crashes on any fmt/module combination */
PROPERTY(emit_no_crash, 10000) {
    reset_log_state();
    log_add_sink(capture_sink, NULL);

    /* Random level 0..4 */
    log_level_t lvl = (log_level_t)ARBI_RANGE(seed, 0, 4);
    char module_buf[16];
    unsigned mlen = (unsigned)ARBI_RANGE(seed, 0, 15);
    ARBI_FILL(seed, (uint8_t *)module_buf, mlen);
    module_buf[mlen] = '\0';

    /* Avoid % in random fmt to prevent UB with variadic args */
    char fmt_buf[32];
    unsigned flen = (unsigned)ARBI_RANGE(seed, 0, 31);
    ARBI_FILL(seed, (uint8_t *)fmt_buf, flen);
    for (unsigned i = 0; i < flen; i++) {
        if (fmt_buf[i] == '%' || fmt_buf[i] == '\0')
            fmt_buf[i] = 'x';
    }
    fmt_buf[flen] = '\0';

    log_emit(lvl, module_buf, fmt_buf);
    /* No crash = pass */
    PROP_ASSERT(1);
}

/* Property: level filtering — messages above runtime level never appear */
PROPERTY(level_filtering, 10000) {
    reset_log_state();
    log_add_sink(capture_sink, NULL);

    log_level_t runtime = (log_level_t)ARBI_RANGE(seed, 0, 4);
    log_set_level(runtime);

    log_level_t emit = (log_level_t)ARBI_RANGE(seed, 1, 4);
    g_cap_len = 0;
    log_emit(emit, "test", "hello");

    if (emit > runtime || runtime == LOG_LEVEL_NONE) {
        PROP_ASSERT_EQ(g_cap_len, 0);
    } else {
        PROP_ASSERT(g_cap_len > 0);
    }
}

/* Property: add/remove sink roundtrip — add then remove always succeeds */
PROPERTY(sink_add_remove, 5000) {
    reset_log_state();

    unsigned n_add = (unsigned)ARBI_RANGE(seed, 1, 4);
    int ids[4];
    for (unsigned i = 0; i < n_add; i++) {
        ids[i] = log_add_sink(capture_sink, NULL);
        PROP_ASSERT(ids[i] >= 0);
    }
    for (unsigned i = 0; i < n_add; i++) {
        int ret = log_remove_sink(ids[i]);
        PROP_ASSERT_EQ(ret, 0);
    }
}

/* Property: output contains module tag and level char */
PROPERTY(output_format, 10000) {
    reset_log_state();
    log_add_sink(capture_sink, NULL);
    log_set_level(LOG_LEVEL_DEBUG);

    log_level_t lvl = (log_level_t)ARBI_RANGE(seed, 1, 4);
    const char *levels = "NEWID";
    char expected_char = levels[(int)lvl];

    g_cap_len = 0;
    log_emit(lvl, "mymod", "msg");

    /* Search for level char in [X] pattern */
    int found_level = 0;
    int found_module = 0;
    for (size_t i = 0; i + 2 < g_cap_len; i++) {
        if (g_captured[i] == '[' && g_captured[i+1] == expected_char && g_captured[i+2] == ']')
            found_level = 1;
    }
    if (g_cap_len > 0) {
        found_module = (strstr(g_captured, "mymod") != NULL) ? 1 : 0;
    }

    PROP_ASSERT(found_level);
    PROP_ASSERT(found_module);
}

TEST_MAIN("log properties") {
    RUN(emit_no_crash);
    RUN(level_filtering);
    RUN(sink_add_remove);
    RUN(output_format);
}
TEST_MAIN_END()
