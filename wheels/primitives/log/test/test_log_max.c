// SPDX-License-Identifier: MIT
/*
 * LOG_LEVEL_MAX compile-time filter.
 *
 * CMakeLists.txt builds this file once per limit and passes the number of
 * levels that must survive, so the expected count cannot come from the same
 * #if it is checking.
 */
#define LOG_MODULE "max"
#include "test_harness.h"
#include "log.h"

#ifndef EXPECTED_LEVELS
#define EXPECTED_LEVELS 4  /* no limit set: all four levels stay */
#endif

static int g_evaluated = 0;
static int g_emitted = 0;

static int side_effect(int x)
{
    g_evaluated++;
    return x;
}

static void counting_sink(void *ctx, const char *line, size_t len)
{
    (void)ctx;
    (void)line;
    (void)len;
    g_emitted++;
}

TEST(test_levels_above_max_are_compiled_out)
{
    (void)side_effect;  /* unused when every level is compiled out */

    log_set_level(LOG_LEVEL_DEBUG);
    ASSERT_TRUE(log_add_sink(counting_sink, NULL) >= 0);

    LOG_DEBUG("MARKER_DEBUG %d", side_effect(1));
    LOG_INFO("MARKER_INFO %d", side_effect(2));
    LOG_WARN("MARKER_WARN %d", side_effect(3));
    LOG_ERROR("MARKER_ERROR %d", side_effect(4));

    ASSERT_EQ(g_evaluated, EXPECTED_LEVELS);
    ASSERT_EQ(g_emitted, EXPECTED_LEVELS);
}

TEST_MAIN("log_max")
    RUN(test_levels_above_max_are_compiled_out);
TEST_MAIN_END()
