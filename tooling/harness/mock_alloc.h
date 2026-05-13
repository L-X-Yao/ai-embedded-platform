/*
 * mock_alloc.h — heap-allocation tripwire for wheel tests
 *
 * Header-only. Codifies the wheel-contract rule "no heap allocation"
 * (ADR / docs/wheel-contract.md) as a runtime assertion.
 *
 * Two modes:
 *   1. NO_ALLOC_BEGIN/END  — bracket a region; any malloc/calloc/realloc/free
 *                            call inside the bracket sets a fail flag.
 *   2. ASSERT_NO_ALLOC     — convenience assert that no allocation has
 *                            happened since the last reset.
 *
 * Off by default — including this header alone is a no-op. To activate
 * tripwire in a CMakeLists.txt, both compile and link flags are needed:
 *
 *   target_compile_definitions(test_xxx PRIVATE MOCK_ALLOC_WRAP_ENABLED)
 *   target_link_options(test_xxx PRIVATE
 *       -Wl,--wrap=malloc -Wl,--wrap=calloc
 *       -Wl,--wrap=realloc -Wl,--wrap=free)
 *
 * Usage:
 *   #include "test_harness.h"
 *   #include "mock_alloc.h"
 *
 *   TEST(my_wheel_does_not_alloc) {
 *       NO_ALLOC_BEGIN();
 *       my_wheel_init(...);
 *       my_wheel_step(...);
 *       NO_ALLOC_END();
 *       ASSERT_EQ(mock_alloc_count, 0);
 *   }
 */

#ifndef MOCK_ALLOC_H
#define MOCK_ALLOC_H

#include <stddef.h>
#include <stdlib.h>

static int mock_alloc_count;
static int mock_free_count;
static int mock_alloc_armed;

static void mock_alloc_reset(void)
{
    mock_alloc_count = 0;
    mock_free_count  = 0;
    mock_alloc_armed = 0;
}

#define NO_ALLOC_BEGIN() do { \
    mock_alloc_count = 0; \
    mock_free_count  = 0; \
    mock_alloc_armed = 1; \
    } while (0)

#define NO_ALLOC_END() do { \
    mock_alloc_armed = 0; \
    } while (0)

/* --- __wrap_* hooks (only compiled with MOCK_ALLOC_WRAP_ENABLED) -------- */

#ifdef MOCK_ALLOC_WRAP_ENABLED

extern void *__real_malloc(size_t);
extern void *__real_calloc(size_t, size_t);
extern void *__real_realloc(void *, size_t);
extern void  __real_free(void *);

void *__wrap_malloc(size_t n)
{
    if (mock_alloc_armed) mock_alloc_count++;
    return __real_malloc(n);
}

void *__wrap_calloc(size_t a, size_t b)
{
    if (mock_alloc_armed) mock_alloc_count++;
    return __real_calloc(a, b);
}

void *__wrap_realloc(void *p, size_t n)
{
    if (mock_alloc_armed) mock_alloc_count++;
    return __real_realloc(p, n);
}

void __wrap_free(void *p)
{
    if (mock_alloc_armed) mock_free_count++;
    __real_free(p);
}

#endif /* MOCK_ALLOC_WRAP_ENABLED */

#endif /* MOCK_ALLOC_H */
