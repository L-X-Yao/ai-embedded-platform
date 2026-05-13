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
#include <string.h>

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
    if (!(_a >= _b - _t && _a <= _b + _t)) { \
        printf("  FAIL %s:%d: %.6f not near %.6f (tol=%.6f)\n", \
               __func__, __LINE__, (double)_a, (double)_b, (double)_t); \
        tests_fail++; return; \
    } } while(0)

/* String-prefix comparison (uses strlen -- stops at NUL).
   For binary data with embedded NUL, use ASSERT_MEMEQ_N instead. */
#define ASSERT_MEMEQ(p, s) do { \
    const char *_s = (s); \
    if (memcmp((p), _s, strlen(_s)) != 0) { \
        printf("  FAIL %s:%d: memcmp mismatch\n", __func__, __LINE__); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_NEAR_D(a, b, eps) do { \
    double _a = (double)(a), _b = (double)(b), _e = (double)(eps); \
    if (!(_a >= _b - _e && _a <= _b + _e)) { \
        printf("  FAIL %s:%d: %g not near %g (eps=%g)\n", \
               __func__, __LINE__, _a, _b, _e); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_MEMEQ_N(p, q, n) do { \
    const unsigned char *_p = (const unsigned char *)(p); \
    const unsigned char *_q = (const unsigned char *)(q); \
    size_t _n = (size_t)(n); \
    size_t _i; \
    for (_i = 0; _i < _n; _i++) { \
        if (_p[_i] != _q[_i]) { \
            printf("  FAIL %s:%d: byte[%zu] 0x%02x != 0x%02x\n", \
                   __func__, __LINE__, _i, _p[_i], _q[_i]); \
            tests_fail++; return; \
        } \
    } } while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("  FAIL %s:%d: expected non-NULL\n", __func__, __LINE__); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        printf("  FAIL %s:%d: expected NULL\n", __func__, __LINE__); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_GT(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (!(_a > _b)) { \
        printf("  FAIL %s:%d: %lld not > %lld\n", __func__, __LINE__, _a, _b); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_GE(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (!(_a >= _b)) { \
        printf("  FAIL %s:%d: %lld not >= %lld\n", __func__, __LINE__, _a, _b); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_LT(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (!(_a < _b)) { \
        printf("  FAIL %s:%d: %lld not < %lld\n", __func__, __LINE__, _a, _b); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_LE(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (!(_a <= _b)) { \
        printf("  FAIL %s:%d: %lld not <= %lld\n", __func__, __LINE__, _a, _b); \
        tests_fail++; return; \
    } } while(0)

#define ASSERT_STR_EQ(a, b) do { \
    const char *_a = (const char *)(a), *_b = (const char *)(b); \
    if (_a == NULL || _b == NULL) { \
        printf("  FAIL %s:%d: ASSERT_STR_EQ with NULL (%s=%s, %s=%s)\n", \
               __func__, __LINE__, #a, _a ? _a : "<NULL>", #b, _b ? _b : "<NULL>"); \
        tests_fail++; return; \
    } \
    if (strcmp(_a, _b) != 0) { \
        printf("  FAIL %s:%d: \"%s\" != \"%s\"\n", __func__, __LINE__, _a, _b); \
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
