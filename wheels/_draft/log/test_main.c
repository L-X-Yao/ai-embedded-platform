/* Test harness for log wheel — runs in VSF linux host sim.
 * Prints PASS/FAIL per case, exits 0 on all pass. */
#include "vsf.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Must be after log.h; each test "module" sets its own */
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

/* ----- test helpers ----- */
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s: %s\n", __func__, msg); \
        g_fail++; \
        return; \
    } \
} while (0)

#define PASS() printf("PASS: %s\n", __func__)

/* ===================== tests ===================== */

static void test_runtime_level_get_set(void) {
    log_set_level(LOG_LEVEL_DEBUG);
    CHECK(log_get_level() == LOG_LEVEL_DEBUG, "set debug");
    log_set_level(LOG_LEVEL_WARN);
    CHECK(log_get_level() == LOG_LEVEL_WARN, "set warn");
    PASS();
}

static void test_add_remove_sink(void) {
    capture_reset();
    /* we've registered capture already */
    CHECK(g_cap_sink_id >= 0, "capture added");

    /* add vsf_trace sink to fill another slot */
    int t = log_add_vsf_trace_sink();
    CHECK(t >= 0, "vsf_trace sink added");

    /* remove vsf_trace sink */
    CHECK(log_remove_sink(t) == 0, "remove ok");
    /* removing twice should fail */
    CHECK(log_remove_sink(t) == -1, "double remove fails");
    /* invalid id */
    CHECK(log_remove_sink(-1) == -1, "invalid id");
    CHECK(log_remove_sink(999) == -1, "oob id");
    PASS();
}

static void test_emit_basic(void) {
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);
    log_set_time_fn(fake_time_ms);
    g_fake_ms = 0;

    #define LOG_MODULE "demo"
    LOG_INFO("hello %d", 42);
    #undef LOG_MODULE
    #define LOG_MODULE "test"

    CHECK(g_cap_count == 1, "1 line captured");
    CHECK(strstr(g_cap[0], "[00000000]") != NULL, "timestamp");
    CHECK(strstr(g_cap[0], "[I]") != NULL, "info level char");
    CHECK(strstr(g_cap[0], "[demo]") != NULL, "module");
    CHECK(strstr(g_cap[0], "hello 42") != NULL, "formatted message");
    CHECK(g_cap[0][strlen(g_cap[0])-1] == '\n', "newline");
    PASS();
}

static void test_level_chars(void) {
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);

    #define LOG_MODULE "lvl"
    LOG_ERROR("err");
    LOG_WARN("warn");
    LOG_INFO("info");
    LOG_DEBUG("dbg");
    #undef LOG_MODULE
    #define LOG_MODULE "test"

    CHECK(g_cap_count == 4, "4 lines");
    CHECK(strstr(g_cap[0], "[E]") && strstr(g_cap[0], "err"),   "error");
    CHECK(strstr(g_cap[1], "[W]") && strstr(g_cap[1], "warn"),  "warn");
    CHECK(strstr(g_cap[2], "[I]") && strstr(g_cap[2], "info"),  "info");
    CHECK(strstr(g_cap[3], "[D]") && strstr(g_cap[3], "dbg"),   "debug");
    PASS();
}

static void test_runtime_level_filter(void) {
    capture_reset();
    log_set_level(LOG_LEVEL_WARN);   /* INFO/DEBUG should be dropped */

    #define LOG_MODULE "filter"
    LOG_DEBUG("dropped");
    LOG_INFO("dropped");
    LOG_WARN("kept");
    LOG_ERROR("kept");
    #undef LOG_MODULE
    #define LOG_MODULE "test"

    CHECK(g_cap_count == 2, "only 2 lines pass filter");
    CHECK(strstr(g_cap[0], "[W]") != NULL, "warn kept");
    CHECK(strstr(g_cap[1], "[E]") != NULL, "error kept");
    PASS();
}

static void test_level_none_disables_all(void) {
    capture_reset();
    log_set_level(LOG_LEVEL_NONE);

    #define LOG_MODULE "off"
    LOG_ERROR("silenced");
    LOG_DEBUG("silenced");
    #undef LOG_MODULE
    #define LOG_MODULE "test"

    CHECK(g_cap_count == 0, "none means zero output");
    PASS();
}

static void test_multi_sink(void) {
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);

    /* second capture buffer via a second sink */
    static char second_buf[256] = {0};
    static int  second_len = 0;
    static int  second_calls = 0;

    void (*second_fn)(void *, const char *, size_t) =
        (void (*)(void *, const char *, size_t))
        (uintptr_t)&second_buf;  /* placeholder to silence static analyzers */
    (void)second_fn;

    /* use a simple local callback */
    struct second_ctx { int calls; char last[256]; };
    static struct second_ctx sctx;
    sctx.calls = 0;
    sctx.last[0] = 0;

    /* lambda-like inline: define a helper with extern-C linkage */
    extern void _test_second_sink(void *ctx, const char *line, size_t len);
    int sid = log_add_sink(_test_second_sink, &sctx);
    CHECK(sid >= 0, "second sink added");

    #define LOG_MODULE "multi"
    LOG_INFO("x");
    #undef LOG_MODULE
    #define LOG_MODULE "test"

    CHECK(g_cap_count == 1, "capture got 1");
    CHECK(sctx.calls == 1, "second got 1");
    CHECK(strstr(sctx.last, "[multi]") != NULL, "second got module");

    log_remove_sink(sid);
    (void)second_len;
    PASS();
}

/* second sink impl */
struct second_ctx_t { int calls; char last[256]; };
void _test_second_sink(void *ctx, const char *line, size_t len) {
    struct second_ctx_t *c = ctx;
    c->calls++;
    size_t n = len < sizeof(c->last) - 1 ? len : sizeof(c->last) - 1;
    memcpy(c->last, line, n);
    c->last[n] = '\0';
}

static void test_no_sinks_no_crash(void) {
    /* remove all sinks */
    for (int i = 0; i < 8; i++) log_remove_sink(i);
    g_cap_sink_id = -1;
    log_set_level(LOG_LEVEL_DEBUG);

    #define LOG_MODULE "silent"
    LOG_INFO("nobody hears me");   /* must not crash */
    #undef LOG_MODULE
    #define LOG_MODULE "test"

    /* Re-add capture for subsequent tests */
    capture_reset();
    PASS();
}

static void test_time_fn_disable(void) {
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);
    log_set_time_fn(NULL);   /* disable timestamp */

    #define LOG_MODULE "notime"
    LOG_INFO("no ts");
    #undef LOG_MODULE
    #define LOG_MODULE "test"

    CHECK(g_cap_count == 1, "1 line");
    /* should start with [I] not [00000000] */
    CHECK(g_cap[0][0] == '[' && g_cap[0][1] == 'I', "starts with level char");
    CHECK(strstr(g_cap[0], "[00") == NULL, "no timestamp prefix");
    log_set_time_fn(fake_time_ms);  /* restore */
    PASS();
}

/* Side-effect counter: proves compile-time filter either evaluates or
 * doesn't evaluate its args. */
static int g_se_counter = 0;
static int side_effect(int x) { g_se_counter++; return x; }

static void test_side_effect_at_default_level(void) {
    /* At default LOG_LEVEL_MAX=LOG_LEVEL_DEBUG, all LOG_* macros expand
     * to a real call → side effect IS evaluated. */
    g_se_counter = 0;
    capture_reset();
    log_set_level(LOG_LEVEL_DEBUG);

    #define LOG_MODULE "se"
    LOG_DEBUG("dbg %d", side_effect(1));
    LOG_INFO("info %d",  side_effect(2));
    LOG_WARN("warn %d",  side_effect(3));
    LOG_ERROR("err %d",  side_effect(4));
    #undef LOG_MODULE
    #define LOG_MODULE "test"

#if LOG_LEVEL_MAX >= LOG_LEVEL_DEBUG
    CHECK(g_se_counter == 4, "debug max: all 4 args evaluated");
#elif LOG_LEVEL_MAX >= LOG_LEVEL_INFO
    CHECK(g_se_counter == 3, "info max: 3 args evaluated, debug gone");
#elif LOG_LEVEL_MAX >= LOG_LEVEL_WARN
    CHECK(g_se_counter == 2, "warn max: 2 args evaluated, debug/info gone");
#elif LOG_LEVEL_MAX >= LOG_LEVEL_ERROR
    CHECK(g_se_counter == 1, "error max: 1 arg evaluated");
#else
    CHECK(g_se_counter == 0, "none: nothing evaluated");
#endif
    PASS();
}

static void test_sink_slots_exhausted(void) {
    /* remove existing */
    for (int i = 0; i < 8; i++) log_remove_sink(i);
    g_cap_sink_id = -1;

    /* LOG_MAX_SINKS default 4: fill 4, expect 5th to fail */
    int ids[6];
    for (int i = 0; i < 6; i++) {
        ids[i] = log_add_sink(capture_sink, NULL);
    }
    CHECK(ids[0] >= 0 && ids[1] >= 0 && ids[2] >= 0 && ids[3] >= 0, "4 slots ok");
    CHECK(ids[4] == -1 && ids[5] == -1, "5th/6th rejected");

    /* cleanup */
    for (int i = 0; i < 4; i++) log_remove_sink(ids[i]);
    capture_reset();
    PASS();
}

/* ===================== runner ===================== */

static void run_tests(vsf_eda_t *eda, vsf_evt_t evt) {
    if (evt == VSF_EVT_INIT) {
        log_set_time_fn(fake_time_ms);

        capture_reset();  /* adds first capture sink */

        test_runtime_level_get_set();
        test_add_remove_sink();
        test_emit_basic();
        test_level_chars();
        test_runtime_level_filter();
        test_level_none_disables_all();
        test_multi_sink();
        test_no_sinks_no_crash();
        test_time_fn_disable();
        test_side_effect_at_default_level();
        test_sink_slots_exhausted();

        printf("DONE: %d failures\n", g_fail);
        fflush(stdout);
        exit(g_fail == 0 ? 0 : 1);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    static vsf_eda_t eda;
    const vsf_eda_cfg_t cfg = {
        .fn.evthandler = run_tests,
        .priority = vsf_prio_0,
    };
    vsf_eda_start(&eda, (vsf_eda_cfg_t *)&cfg);
    return 0;
}
