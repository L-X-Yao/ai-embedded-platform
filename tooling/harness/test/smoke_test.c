/*
 * smoke_test.c — sanity check that test_harness.h, mock_spi.h, and
 * mock_alloc.h compile and behave correctly with the new features.
 *
 * Build (host):
 *   gcc -std=c99 -Wall -Wextra -Werror -I.. smoke_test.c -o smoke
 *   ./smoke
 *
 * The smoke binary must finish with non-zero failures = 0, at least one
 * skipped test, and BEFORE/AFTER counters matching RUN counts.
 */

#include "test_harness.h"
#include "mock_spi.h"
#include "mock_alloc.h"

static int before_calls;
static int after_calls;
static int explicit_setup_calls;

BEFORE_EACH() {
    before_calls++;
}

AFTER_EACH() {
    after_calls++;
}

static void explicit_setup(void) {
    explicit_setup_calls++;
}

TEST(test_basic_assert) {
    ASSERT_EQ(1 + 1, 2);
    ASSERT_TRUE(1);
    ASSERT_FALSE(0);
}

TEST(test_skip_works) {
    SKIP("smoke");
    /* not reached */
    ASSERT_EQ(1, 0);
}

TEST(test_fail_compiles) {
    /* exercise FAIL macro under a never-taken branch so smoke passes */
    if (0) FAIL("never");
    ASSERT_TRUE(1);
}

TEST(test_run_f_calls_setup) {
    ASSERT_GT(explicit_setup_calls, 0);
}

TEST(test_spi_matchers) {
    mock_spi_reset();
    mock_spi_log_byte(0x06);
    mock_spi_log_byte(0x02);
    mock_spi_log_byte(0x12);
    mock_spi_log_byte(0x34);

    uint8_t prefix[] = {0x06, 0x02};
    uint8_t exact[]  = {0x06, 0x02, 0x12, 0x34};
    uint8_t mid[]    = {0x02, 0x12};
    uint8_t miss[]   = {0xFF};

    ASSERT_EQ(mock_spi_log_starts_with(prefix, 2), 1);
    ASSERT_EQ(mock_spi_log_equals(exact, 4), 1);
    ASSERT_EQ(mock_spi_log_contains(mid, 2), 1);
    ASSERT_EQ(mock_spi_log_starts_with(miss, 1), 0);
    ASSERT_EQ(mock_spi_log_equals(prefix, 2), 0);
}

TEST(test_alloc_counters_safe_noop) {
    /* mock_alloc.h must compile and have working counters even when
     * the linker --wrap flags are not set (no-op mode). */
    mock_alloc_reset();
    NO_ALLOC_BEGIN();
    ASSERT_EQ(mock_alloc_armed, 1);
    NO_ALLOC_END();
    ASSERT_EQ(mock_alloc_armed, 0);
    ASSERT_EQ(mock_alloc_count, 0);
}

TEST(test_new_asserts) {
    ASSERT_GT(5, 3);
    ASSERT_GE(5, 5);
    ASSERT_LT(3, 5);
    ASSERT_LE(5, 5);
    int *p = NULL;
    ASSERT_NULL(p);
    int x = 0;
    ASSERT_NOT_NULL(&x);
}

TEST_MAIN("harness_smoke") {
    RUN(test_basic_assert);
    RUN(test_skip_works);
    RUN(test_fail_compiles);
    RUN_F(test_run_f_calls_setup, explicit_setup);
    RUN(test_spi_matchers);
    RUN(test_alloc_counters_safe_noop);
    RUN(test_new_asserts);

    /* extra invariants — print outside the suite for visibility */
    printf("\nfixtures: before_calls=%d after_calls=%d setup=%d skipped=%d\n",
           before_calls, after_calls, explicit_setup_calls, tests_skip);

    /* before/after must fire for every RUN/RUN_F (7 of each) */
    if (before_calls != 7) { printf("BEFORE_EACH count wrong\n"); tests_fail++; }
    if (after_calls  != 7) { printf("AFTER_EACH count wrong\n");  tests_fail++; }
    if (explicit_setup_calls != 1) { printf("RUN_F setup count wrong\n"); tests_fail++; }
    if (tests_skip != 1) { printf("expected 1 skipped test\n"); tests_fail++; }
}
TEST_MAIN_END()
