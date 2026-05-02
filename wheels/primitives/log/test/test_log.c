// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "log.h"

#include <string.h>

#define LOG_MODULE "test"

/* ----- capture sink: stores lines for assertion ----- */
#define CAP_LINES 32
#define CAP_LEN   256

static char  g_cap[CAP_LINES][CAP_LEN];
static int   g_cap_count = 0;
static int   g_cap_sink_id = -1;

static void capture_sink(void *ctx, const char *line, size_t len) {
    (void)ctx;
    if (g_cap_count < CAP_LINES) {
        size_t n = len < CAP_LEN - 1 ? len : CAP_LEN - 1;
        memcpy(g_cap[g_cap_count], line, n);
        g_cap[g_cap_count][n] = '\0';
        g_cap_count++;
    }
}

static void capture_reset(void) {
    g_cap_count = 0;
    memset(g_cap, 0, sizeof(g_cap));
    if (g_cap_sink_id >= 0) {
        log_remove_sink(g_cap_sink_id);
        g_cap_sink_id = -1;
    }
    g_cap_sink_id = log_add_sink(capture_sink, NULL);
}

/* ----- fixed time fn for deterministic timestamps ----- */
static uint32_t g_fake_ms = 0;
static uint32_t fake_time_ms(void) { return g_fake_ms; }

/* ----- second sink for multi-sink test ----- */
static int g_second_calls = 0;
static char g_second_last[256];

static void second_sink(void *ctx, const char *line, size_t len) {
    (void)ctx;
    g_second_calls++;
    size_t n = len < sizeof(g_second_last) - 1 ? len : sizeof(g_second_last) - 1;
    memcpy(g_second_last, line, n);
    g_second_last[n] = '\0';
}

/* ===================== tests ===================== */

TEST(test_runtime_level_get_set) {
    log_set_level(LOG_LEVEL_DEBUG);
    ASSERT_EQ(log_get_level(), LOG_LEVEL_DEBUG);
    log_set_level(LOG_LEVEL_WARN);
    ASSERT_EQ(log_get_level(), LOG_LEVEL_WARN);
}

TEST(test_add_remove_sink) {
    capture_reset();
    ASSERT_TRUE(g_cap_sink_id >= 0);

    int extra = log_add_sink(second_sink, NULL);
    ASSERT_TRUE(extra >= 0);
    ASSERT_EQ(log_remove_sink(extra), 0);
    ASSERT_EQ(log_remove_sink(extra), -1);
    ASSERT_EQ(log_remove_sink(-1), -1);
    ASSERT_EQ(log_remove_sink(999), -1);
}

TEST(test_emit_basic) {
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);
    log_set_time_fn(fake_time_ms);
    g_fake_ms = 0;

#undef LOG_MODULE
#define LOG_MODULE "demo"
    LOG_INFO("hello %d", 42);
#undef LOG_MODULE
#define LOG_MODULE "test"

    ASSERT_EQ(g_cap_count, 1);
    ASSERT_TRUE(strstr(g_cap[0], "[00000000]") != NULL);
    ASSERT_TRUE(strstr(g_cap[0], "[I]") != NULL);
    ASSERT_TRUE(strstr(g_cap[0], "[demo]") != NULL);
    ASSERT_TRUE(strstr(g_cap[0], "hello 42") != NULL);
    ASSERT_EQ(g_cap[0][strlen(g_cap[0]) - 1], '\n');
}

TEST(test_level_chars) {
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);

#undef LOG_MODULE
#define LOG_MODULE "lvl"
    LOG_ERROR("err");
    LOG_WARN("warn");
    LOG_INFO("info");
    LOG_DEBUG("dbg");
#undef LOG_MODULE
#define LOG_MODULE "test"

    ASSERT_EQ(g_cap_count, 4);
    ASSERT_TRUE(strstr(g_cap[0], "[E]") != NULL);
    ASSERT_TRUE(strstr(g_cap[1], "[W]") != NULL);
    ASSERT_TRUE(strstr(g_cap[2], "[I]") != NULL);
    ASSERT_TRUE(strstr(g_cap[3], "[D]") != NULL);
}

TEST(test_runtime_level_filter) {
    capture_reset();
    log_set_level(LOG_LEVEL_WARN);

#undef LOG_MODULE
#define LOG_MODULE "filter"
    LOG_DEBUG("dropped");
    LOG_INFO("dropped");
    LOG_WARN("kept");
    LOG_ERROR("kept");
#undef LOG_MODULE
#define LOG_MODULE "test"

    ASSERT_EQ(g_cap_count, 2);
    ASSERT_TRUE(strstr(g_cap[0], "[W]") != NULL);
    ASSERT_TRUE(strstr(g_cap[1], "[E]") != NULL);
}

TEST(test_level_none_disables_all) {
    capture_reset();
    log_set_level(LOG_LEVEL_NONE);

#undef LOG_MODULE
#define LOG_MODULE "off"
    LOG_ERROR("silenced");
    LOG_DEBUG("silenced");
#undef LOG_MODULE
#define LOG_MODULE "test"

    ASSERT_EQ(g_cap_count, 0);
}

TEST(test_multi_sink) {
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);
    g_second_calls = 0;
    g_second_last[0] = '\0';

    int sid = log_add_sink(second_sink, NULL);
    ASSERT_TRUE(sid >= 0);

#undef LOG_MODULE
#define LOG_MODULE "multi"
    LOG_INFO("x");
#undef LOG_MODULE
#define LOG_MODULE "test"

    ASSERT_EQ(g_cap_count, 1);
    ASSERT_EQ(g_second_calls, 1);
    ASSERT_TRUE(strstr(g_second_last, "[multi]") != NULL);
    log_remove_sink(sid);
}

TEST(test_no_sinks_no_crash) {
    for (int i = 0; i < 8; i++) log_remove_sink(i);
    g_cap_sink_id = -1;
    log_set_level(LOG_LEVEL_DEBUG);

#undef LOG_MODULE
#define LOG_MODULE "silent"
    LOG_INFO("nobody hears me");
#undef LOG_MODULE
#define LOG_MODULE "test"

    capture_reset();
}

TEST(test_time_fn_disable) {
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);
    log_set_time_fn(NULL);

#undef LOG_MODULE
#define LOG_MODULE "notime"
    LOG_INFO("no ts");
#undef LOG_MODULE
#define LOG_MODULE "test"

    ASSERT_EQ(g_cap_count, 1);
    ASSERT_EQ(g_cap[0][0], '[');
    ASSERT_EQ(g_cap[0][1], 'I');
    ASSERT_TRUE(strstr(g_cap[0], "[00") == NULL);
    log_set_time_fn(fake_time_ms);
}

static int g_se_counter = 0;
static int side_effect(int x) { g_se_counter++; return x; }

TEST(test_side_effect_at_default_level) {
    g_se_counter = 0;
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);

#undef LOG_MODULE
#define LOG_MODULE "se"
    LOG_DEBUG("dbg %d", side_effect(1));
    LOG_INFO("info %d",  side_effect(2));
    LOG_WARN("warn %d",  side_effect(3));
    LOG_ERROR("err %d",  side_effect(4));
#undef LOG_MODULE
#define LOG_MODULE "test"

#if LOG_LEVEL_MAX >= LOG_LEVEL_DEBUG
    ASSERT_EQ(g_se_counter, 4);
#elif LOG_LEVEL_MAX >= LOG_LEVEL_INFO
    ASSERT_EQ(g_se_counter, 3);
#elif LOG_LEVEL_MAX >= LOG_LEVEL_WARN
    ASSERT_EQ(g_se_counter, 2);
#elif LOG_LEVEL_MAX >= LOG_LEVEL_ERROR
    ASSERT_EQ(g_se_counter, 1);
#else
    ASSERT_EQ(g_se_counter, 0);
#endif
}

TEST(test_stdout_sink) {
    for (int i = 0; i < 8; i++) log_remove_sink(i);
    g_cap_sink_id = -1;
    log_set_level(LOG_LEVEL_DEBUG);

    int sid = log_add_stdout_sink();
    ASSERT_TRUE(sid >= 0);

#undef LOG_MODULE
#define LOG_MODULE "stdout"
    LOG_INFO("stdout sink works");
#undef LOG_MODULE
#define LOG_MODULE "test"

    ASSERT_EQ(log_remove_sink(sid), 0);
    capture_reset();
}

TEST(test_sink_slots_exhausted) {
    for (int i = 0; i < 8; i++) log_remove_sink(i);
    g_cap_sink_id = -1;

    int ids[6];
    for (int i = 0; i < 6; i++) {
        ids[i] = log_add_sink(capture_sink, NULL);
    }
    ASSERT_TRUE(ids[0] >= 0);
    ASSERT_TRUE(ids[1] >= 0);
    ASSERT_TRUE(ids[2] >= 0);
    ASSERT_TRUE(ids[3] >= 0);
    ASSERT_EQ(ids[4], -1);
    ASSERT_EQ(ids[5], -1);

    for (int i = 0; i < 4; i++) log_remove_sink(ids[i]);
    capture_reset();
}

TEST_MAIN("log")
    log_set_time_fn(fake_time_ms);
    capture_reset();

    RUN(test_runtime_level_get_set);
    RUN(test_add_remove_sink);
    RUN(test_emit_basic);
    RUN(test_level_chars);
    RUN(test_runtime_level_filter);
    RUN(test_level_none_disables_all);
    RUN(test_multi_sink);
    RUN(test_no_sinks_no_crash);
    RUN(test_time_fn_disable);
    RUN(test_side_effect_at_default_level);
    RUN(test_stdout_sink);
    RUN(test_sink_slots_exhausted);
TEST_MAIN_END()
