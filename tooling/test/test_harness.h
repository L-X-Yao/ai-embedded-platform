/*
 * test_harness.h — unified C test harness for all wheels
 *
 * Header-only. Include from any test_*.c file.
 * Provides: ASSERT macros, RUN() runner, tests_pass/tests_fail counters.
 *
 * Usage:
 *   #include "test_harness.h"
 *
 *   TEST(test_something) {
 *       ASSERT_EQ(1 + 1, 2);
 *       ASSERT_TRUE(some_condition);
 *   }
 *
 *   TEST_MAIN() {
 *       RUN(test_something);
 *   }
 *   TEST_MAIN_END()
 */

#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>

static int tests_pass = 0;
static int tests_fail = 0;

#define TEST(name) static void name(void)

#define ASSERT_EQ(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { \
        printf("  FAIL %s:%d: %lld != %lld\n", __func__, __LINE__, _a, _b); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_EQ_HEX(a, b) do { \
    unsigned long long _a = (unsigned long long)(a), _b = (unsigned long long)(b); \
    if (_a != _b) { \
        printf("  FAIL %s:%d: 0x%llX != 0x%llX\n", __func__, __LINE__, _a, _b); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_TRUE(x) ASSERT_EQ(!!(x), 1)
#define ASSERT_FALSE(x) ASSERT_EQ(!!(x), 0)

#define ASSERT_NEAR(a, b, tol) do { \
    float _a = (float)(a), _b = (float)(b), _t = (float)(tol); \
    if ((_a - _b) > _t || (_b - _a) > _t) { \
        printf("  FAIL %s:%d: %.6f not near %.6f (tol=%.6f)\n", \
               __func__, __LINE__, (double)_a, (double)_b, (double)_t); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_MEMEQ(p, s) do { \
    if (memcmp((p), (s), strlen(s)) != 0) { \
        printf("  FAIL %s:%d: memcmp mismatch\n", __func__, __LINE__); \
        tests_fail++; return; \
    } } while(0)

#define RUN(fn) do { \
    int _pf = tests_fail; \
    fn(); \
    if (tests_fail == _pf) { printf("  PASS %s\n", #fn); tests_pass++; } \
    } while(0)

#define TEST_MAIN(suite_name) \
    int main(void) { \
    printf("%s tests:\n", suite_name);

#define TEST_MAIN_END() \
    printf("\n%d passed, %d failed\n", tests_pass, tests_fail); \
    return tests_fail ? 1 : 0; \
    }

#endif /* TEST_HARNESS_H */
