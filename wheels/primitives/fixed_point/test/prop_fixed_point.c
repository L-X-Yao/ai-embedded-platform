/*
 * prop_fixed_point.c — property-based tests for fixed_point
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_fixed_point.c ../fixed_point.c -o prop_fixed_point
 */

#include "test_harness.h"
#include "property_test.h"
#include "../fixed_point.h"

/* Property: int -> fp -> int roundtrip preserves value within representable range */
PROPERTY(int_roundtrip, 10000) {
    int16_t val = (int16_t)ARBI_RANGE(seed, -32768, 32767);
    fp_t fp = fp_from_int(val);
    int16_t back = fp_to_int(fp);
    PROP_ASSERT_EQ(back, val);
}

/* Property: addition is commutative: fp_add(a,b) == fp_add(b,a) */
PROPERTY(add_commutative, 10000) {
    /* Use smaller range to avoid saturation making test trivial */
    int16_t ai = (int16_t)ARBI_RANGE(seed, -1000, 1000);
    int16_t bi = (int16_t)ARBI_RANGE(seed, -1000, 1000);
    fp_t a = fp_from_int(ai);
    fp_t b = fp_from_int(bi);
    fp_t ab = fp_add(a, b);
    fp_t ba = fp_add(b, a);
    PROP_ASSERT_EQ(ab, ba);
}

/* Property: multiplication result bounded — small int * small int stays in range */
PROPERTY(mul_bounded, 10000) {
    int16_t ai = (int16_t)ARBI_RANGE(seed, -180, 180);
    int16_t bi = (int16_t)ARBI_RANGE(seed, -180, 180);
    fp_t a = fp_from_int(ai);
    fp_t b = fp_from_int(bi);
    fp_t result = fp_mul(a, b);

    /* Product of two values in [-180,180] fits in int32 as fixed-point */
    int32_t expected_max = 180 * 180;  /* 32400 < 32767 */
    PROP_ASSERT(fp_to_int(fp_abs(result)) <= expected_max);
}

/* Property: fp_clamp output always within [lo, hi] */
PROPERTY(clamp_bounded, 10000) {
    int16_t v = (int16_t)ARBI_RANGE(seed, -32768, 32767);
    int16_t lo_i = (int16_t)ARBI_RANGE(seed, -1000, 0);
    int16_t hi_i = (int16_t)ARBI_RANGE(seed, 1, 1000);
    fp_t val = fp_from_int(v);
    fp_t lo = fp_from_int(lo_i);
    fp_t hi = fp_from_int(hi_i);
    fp_t clamped = fp_clamp(val, lo, hi);
    PROP_ASSERT(clamped >= lo);
    PROP_ASSERT(clamped <= hi);
}

/* Property: no-crash on arbitrary raw fp_t values for all operations */
PROPERTY(no_crash_arbitrary, 50000) {
    fp_t a = (fp_t)ARBI_UINT32(seed);
    fp_t b = (fp_t)ARBI_UINT32(seed);

    /* None of these should crash */
    (void)fp_add(a, b);
    (void)fp_sub(a, b);
    (void)fp_mul(a, b);
    (void)fp_div(a, b);
    (void)fp_abs(a);
    (void)fp_min(a, b);
    (void)fp_max(a, b);
    (void)fp_gt(a, b);
    (void)fp_lt(a, b);
    (void)fp_eq(a, b);
    (void)fp_to_int(a);
    (void)fp_to_float(a);

    char buf[32];
    (void)fp_to_string(a, buf, sizeof(buf), (uint8_t)(ARBI_UINT8(seed) % 6));
    PROP_ASSERT(1); /* reached without crash */
}

TEST_MAIN("fixed_point properties") {
    RUN(int_roundtrip);
    RUN(add_commutative);
    RUN(mul_bounded);
    RUN(clamp_bounded);
    RUN(no_crash_arbitrary);
}
TEST_MAIN_END()
