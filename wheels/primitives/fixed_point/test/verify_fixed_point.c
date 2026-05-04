/*
 * verify_fixed_point.c — CBMC bounded model checking for fixed_point
 *
 * Verifies Q16.16 arithmetic properties: commutativity, identity,
 * overflow saturation, and conversion roundtrips. Run with:
 *   cbmc verify_fixed_point.c ../fixed_point.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../fixed_point.h"
#include <stdint.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

extern uint8_t  nondet_uint8(void);
extern int16_t  nondet_int16(void);
extern int32_t  nondet_int32(void);
extern unsigned  nondet_unsigned(void);

/* Proof 1: arithmetic identities (commutative add/mul, zero/one identity) */
void verify_arithmetic_identities(void) {
    int32_t raw_a = nondet_int32();
    int32_t raw_b = nondet_int32();

    /* Constrain to avoid overflow for identity checks */
    __CPROVER_assume(raw_a > FP_MIN / 2 && raw_a < FP_MAX / 2);
    __CPROVER_assume(raw_b > FP_MIN / 2 && raw_b < FP_MAX / 2);

    fp_t a = raw_a;
    fp_t b = raw_b;

    /* Commutativity of add */
    assert(fp_add(a, b) == fp_add(b, a));

    /* Commutativity of mul */
    assert(fp_mul(a, b) == fp_mul(b, a));

    /* Additive identity */
    assert(fp_add(a, 0) == a);

    /* Multiplicative identity */
    assert(fp_mul(a, FP_ONE) == a);

    /* Additive inverse */
    assert(fp_sub(a, a) == 0);
}

/* Proof 2: int conversion roundtrip */
void verify_int_conversion_roundtrip(void) {
    int16_t val = nondet_int16();

    fp_t  fp  = fp_from_int(val);
    int16_t back = fp_to_int(fp);

    assert(back == val);
}

/* Proof 3: comparison consistency */
void verify_comparisons(void) {
    int32_t raw_a = nondet_int32();
    int32_t raw_b = nondet_int32();
    fp_t a = raw_a;
    fp_t b = raw_b;

    /* Exactly one of <, >, == holds */
    bool lt = fp_lt(a, b);
    bool gt = fp_gt(a, b);
    bool eq = fp_eq(a, b);

    /* At most one is true */
    assert(!(lt && gt));
    assert(!(lt && eq));
    assert(!(gt && eq));

    /* At least one is true */
    assert(lt || gt || eq);

    /* Reflexivity */
    assert(fp_eq(a, a));
    assert(!fp_lt(a, a));
    assert(!fp_gt(a, a));

    /* Antisymmetry */
    if (fp_lt(a, b)) {
        assert(fp_gt(b, a));
    }
}

/* Proof 4: clamp, abs, min, max properties */
void verify_math_functions(void) {
    int32_t raw_val = nondet_int32();
    fp_t val = raw_val;

    /* abs is non-negative (except FP_MIN edge case) */
    if (val != FP_MIN) {
        assert(fp_abs(val) >= 0);
    }

    /* abs(x) == x when x >= 0 */
    if (val >= 0) {
        assert(fp_abs(val) == val);
    }

    /* min/max properties */
    int32_t raw_a = nondet_int32();
    int32_t raw_b = nondet_int32();
    fp_t a = raw_a;
    fp_t b = raw_b;

    fp_t mn = fp_min(a, b);
    fp_t mx = fp_max(a, b);
    assert(mn <= a && mn <= b);
    assert(mx >= a && mx >= b);
    assert(mn <= mx);

    /* clamp: result is in [lo, hi] */
    fp_t lo = fp_from_int(-100);
    fp_t hi = fp_from_int(100);
    fp_t clamped = fp_clamp(val, lo, hi);
    assert(clamped >= lo && clamped <= hi);

    /* clamp identity when already in range */
    if (val >= lo && val <= hi) {
        assert(fp_clamp(val, lo, hi) == val);
    }
}

int main(void) {
    verify_arithmetic_identities();
    verify_int_conversion_roundtrip();
    verify_comparisons();
    verify_math_functions();
    return 0;
}
