/*
 * property_test.h — property-based testing for wheels
 *
 * Header-only. Extends test_harness.h with deterministic, seed-based
 * property tests. Each property runs N iterations with a reproducible
 * xorshift32 PRNG; on failure the seed is printed for replay.
 *
 * Usage:
 *   #include "test_harness.h"
 *   #include "property_test.h"
 *
 *   PROPERTY(roundtrip, 10000) {
 *       uint8_t buf[256];
 *       size_t len = ARBI_SIZE(seed, 255);
 *       ARBI_FILL(seed, buf, len);
 *       // ... test property ...
 *       PROP_ASSERT(decoded_len == len);
 *   }
 *
 *   TEST_MAIN("props") { RUN(roundtrip); } TEST_MAIN_END()
 */

#ifndef PROPERTY_TEST_H
#define PROPERTY_TEST_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifndef PROP_BASE_SEED
#define PROP_BASE_SEED 0xDEADBEEFu
#endif

/* --- xorshift32 PRNG ---------------------------------------------------- */

static uint32_t prop__xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* --- arbitrary value generators ------------------------------------------ */

#define ARBI_UINT32(s)      prop__xorshift32(&(s))
#define ARBI_UINT16(s)      ((uint16_t)(prop__xorshift32(&(s)) & 0xFFFFu))
#define ARBI_UINT8(s)       ((uint8_t)(prop__xorshift32(&(s)) & 0xFFu))
#define ARBI_SIZE(s, max) \
    ((uint32_t)(max) == UINT32_MAX \
        ? (size_t)prop__xorshift32(&(s)) \
        : (size_t)(prop__xorshift32(&(s)) % ((uint32_t)(max) + 1u)))
#define ARBI_RANGE(s, lo, hi) \
    ((uint32_t)(hi) - (uint32_t)(lo) == UINT32_MAX \
        ? (int)((uint32_t)(lo) + prop__xorshift32(&(s))) \
        : (int)((uint32_t)(lo) + prop__xorshift32(&(s)) % ((uint32_t)(hi) - (uint32_t)(lo) + 1u)))

static void prop__arbi_fill(uint32_t *state, uint8_t *buf, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
        buf[i] = (uint8_t)(prop__xorshift32(state) & 0xFFu);
}

#define ARBI_FILL(s, buf, len) prop__arbi_fill(&(s), (uint8_t *)(buf), (len))

/* --- property assertions (print seed on failure) ------------------------- */

#define PROP_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL %s:%d: %s  [seed=0x%08X iter=%d]\n", \
               __func__, __LINE__, #cond, \
               (unsigned)prop__seed_orig, prop__iter); \
        tests_fail++; return; \
    } } while (0)

#define PROP_ASSERT_EQ(a, b) do { \
    long long _pa = (long long)(a), _pb = (long long)(b); \
    if (_pa != _pb) { \
        printf("  FAIL %s:%d: %lld != %lld  [seed=0x%08X iter=%d]\n", \
               __func__, __LINE__, _pa, _pb, \
               (unsigned)prop__seed_orig, prop__iter); \
        tests_fail++; return; \
    } } while (0)

#define PROP_ASSERT_MEM_EQ(a, b, n) do { \
    if (memcmp((a), (b), (n)) != 0) { \
        printf("  FAIL %s:%d: mem mismatch (%d bytes)  [seed=0x%08X iter=%d]\n", \
               __func__, __LINE__, (int)(n), \
               (unsigned)prop__seed_orig, prop__iter); \
        tests_fail++; return; \
    } } while (0)

/* --- PROPERTY macro ------------------------------------------------------ */

/*
 * Expands to a wrapper that calls the body N times, plus an inner
 * function containing the actual property body. The wrapper is a
 * normal TEST-compatible void(void) so RUN() works unchanged.
 *
 * Variables available inside the body:
 *   uint32_t seed            — mutable PRNG state for this iteration
 *   uint32_t prop__seed_orig — snapshot of seed at iteration start
 *   int      prop__iter      — current iteration index (0-based)
 */

#define PROPERTY(name, iters) \
    static void name##__body(uint32_t seed, uint32_t prop__seed_orig, \
                             int prop__iter); \
    static void name(void) { \
        int _i; \
        for (_i = 0; _i < (iters); _i++) { \
            uint32_t _s = PROP_BASE_SEED ^ (uint32_t)(_i + 1); \
            int _pf = tests_fail; \
            name##__body(_s, _s, _i); \
            if (tests_fail != _pf) return; \
        } \
    } \
    static void name##__body(uint32_t seed, uint32_t prop__seed_orig, \
                             int prop__iter)

#endif /* PROPERTY_TEST_H */
