// SPDX-License-Identifier: MIT
/*
 * contract.h — Design by Contract macros for wheels
 *
 * Header-only. Enable with -DCONTRACT_ENABLED (test builds).
 * In release builds all macros compile to nothing.
 *
 * Usage:
 *   #include "contract.h"
 *
 *   int foo(int *ptr, size_t len) {
 *       REQUIRE(ptr != NULL);
 *       REQUIRE(len > 0 && len <= MAX_LEN);
 *       // ... implementation ...
 *       ENSURE(result >= 0);
 *       return result;
 *   }
 *
 * Override the violation handler by defining CONTRACT_VIOLATION_HANDLER
 * as a function-like macro before including this header:
 *   #define CONTRACT_VIOLATION_HANDLER my_handler
 */

#ifndef CONTRACT_H
#define CONTRACT_H

#ifdef CONTRACT_ENABLED

#include <stdio.h>
#include <stdlib.h>

/*
 * Default violation handler. Override by defining
 * CONTRACT_VIOLATION_HANDLER(kind, cond, file, line) before including
 * this header, or by defining contract_violation_handler() in your
 * test file (the #ifndef guard will skip this default).
 */
#ifndef CONTRACT_VIOLATION_HANDLER
#define CONTRACT_VIOLATION_HANDLER contract__default_handler
static void contract__default_handler(const char *kind, const char *cond,
                                      const char *file, int line)
{
    fprintf(stderr, "CONTRACT %s VIOLATED: %s  [%s:%d]\n",
            kind, cond, file, line);
    abort();
}
#endif

#define REQUIRE(cond) do { \
    if (!(cond)) { \
        CONTRACT_VIOLATION_HANDLER("REQUIRE", #cond, __FILE__, __LINE__); \
    } } while (0)

#define ENSURE(cond) do { \
    if (!(cond)) { \
        CONTRACT_VIOLATION_HANDLER("ENSURE", #cond, __FILE__, __LINE__); \
    } } while (0)

#define INVARIANT(cond) do { \
    if (!(cond)) { \
        CONTRACT_VIOLATION_HANDLER("INVARIANT", #cond, __FILE__, __LINE__); \
    } } while (0)

#define REQUIRE_NOT_NULL(ptr) REQUIRE((ptr) != NULL)

#if defined(__GNUC__) || defined(__clang__)
#define CONTRACT_UNREACHABLE() do { \
    CONTRACT_VIOLATION_HANDLER("UNREACHABLE", "reached unreachable code", \
                               __FILE__, __LINE__); \
    __builtin_unreachable(); \
    } while (0)
#else
#define CONTRACT_UNREACHABLE() do { \
    CONTRACT_VIOLATION_HANDLER("UNREACHABLE", "reached unreachable code", \
                               __FILE__, __LINE__); \
    } while (0)
#endif

#else /* CONTRACT_ENABLED not defined */

#define REQUIRE(cond)           ((void)0)
#define ENSURE(cond)            ((void)0)
#define INVARIANT(cond)         ((void)0)
#define REQUIRE_NOT_NULL(ptr)   ((void)0)

#if defined(__GNUC__) || defined(__clang__)
#define CONTRACT_UNREACHABLE()  __builtin_unreachable()
#else
#define CONTRACT_UNREACHABLE()  ((void)0)
#endif

#endif /* CONTRACT_ENABLED */

#endif /* CONTRACT_H */
