// SPDX-License-Identifier: MIT
/*
 * verify_bitmap.c — CBMC bounded model checking for bitmap
 *
 * Verifies structural invariants, alloc/free consistency, and counting.
 * Run with:
 *   cbmc verify_bitmap.c ../bitmap.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../bitmap.h"
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

#define BM_BITS 32

extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern unsigned  nondet_unsigned(void);

static void check_invariants(const bitmap_t *bm) {
    uint16_t set_count   = bitmap_count_set(bm);
    uint16_t clear_count = bitmap_count_clear(bm);
    assert(set_count + clear_count == bm->bits);
    assert(bitmap_size(bm) == bm->bits);

    if (set_count == 0) {
        assert(bitmap_find_first_set(bm) == -1);
    } else {
        int ffs = bitmap_find_first_set(bm);
        assert(ffs >= 0 && (uint16_t)ffs < bm->bits);
        assert(bitmap_test(bm, (uint16_t)ffs) == true);
    }

    if (clear_count == 0) {
        assert(bitmap_find_first_clear(bm) == -1);
    } else {
        int ffc = bitmap_find_first_clear(bm);
        assert(ffc >= 0 && (uint16_t)ffc < bm->bits);
        assert(bitmap_test(bm, (uint16_t)ffc) == false);
    }
}

/* Proof 1: invariants hold after any sequence of operations */
void verify_invariants(void) {
    uint8_t storage[(BM_BITS + 7) / 8];
    bitmap_t bm;

    bitmap_err_t err = bitmap_init(&bm, storage, BM_BITS);
    assert(err == BITMAP_OK);
    check_invariants(&bm);

    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= UNWIND_BOUND);

    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = nondet_uint8() % 6;
        uint16_t bit = nondet_uint16() % BM_BITS;
        switch (op) {
        case 0: bitmap_set(&bm, bit); break;
        case 1: bitmap_clear(&bm, bit); break;
        case 2: bitmap_toggle(&bm, bit); break;
        case 3: bitmap_set_all(&bm); break;
        case 4: bitmap_clear_all(&bm); break;
        case 5: bitmap_test(&bm, bit); break;
        }
        check_invariants(&bm);
    }
}

/* Proof 2: alloc returns unique slots, free releases them */
void verify_alloc_free(void) {
    uint8_t storage[(BM_BITS + 7) / 8];
    bitmap_t bm;
    bitmap_init(&bm, storage, BM_BITS);

    /* Allocate 4 slots — each must be unique and valid */
    int slots[4];
    for (int i = 0; i < 4; i++) {
        slots[i] = bitmap_alloc(&bm);
        assert(slots[i] >= 0 && (uint16_t)slots[i] < BM_BITS);
        assert(bitmap_test(&bm, (uint16_t)slots[i]) == true);
    }

    /* All slots are distinct */
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            assert(slots[i] != slots[j]);
        }
    }

    assert(bitmap_count_set(&bm) == 4);

    /* Free all and verify */
    for (int i = 0; i < 4; i++) {
        bitmap_err_t err = bitmap_free(&bm, (uint16_t)slots[i]);
        assert(err == BITMAP_OK);
        assert(bitmap_test(&bm, (uint16_t)slots[i]) == false);
    }

    assert(bitmap_count_set(&bm) == 0);
}

/* Proof 3: set/clear/toggle are consistent with test */
void verify_set_clear_toggle(void) {
    uint8_t storage[(BM_BITS + 7) / 8];
    bitmap_t bm;
    bitmap_init(&bm, storage, BM_BITS);

    uint16_t bit = nondet_uint16();
    __CPROVER_assume(bit < BM_BITS);

    /* After init all bits are clear */
    assert(bitmap_test(&bm, bit) == false);

    /* Set makes it true */
    bitmap_set(&bm, bit);
    assert(bitmap_test(&bm, bit) == true);

    /* Toggle flips it back */
    bitmap_toggle(&bm, bit);
    assert(bitmap_test(&bm, bit) == false);

    /* Toggle again sets it */
    bitmap_toggle(&bm, bit);
    assert(bitmap_test(&bm, bit) == true);

    /* Clear makes it false */
    bitmap_clear(&bm, bit);
    assert(bitmap_test(&bm, bit) == false);
}

/* Proof 4: NULL and out-of-range safety */
void verify_null_safety(void) {
    bitmap_err_t err;

    err = bitmap_init(NULL, NULL, 0);
    assert(err == BITMAP_ERR_PARAM);

    uint8_t storage[4];
    bitmap_t bm;
    err = bitmap_init(&bm, storage, BM_BITS);
    assert(err == BITMAP_OK);

    /* Out-of-range bit */
    err = bitmap_set(&bm, BM_BITS + 1);
    assert(err == BITMAP_ERR_RANGE);

    err = bitmap_clear(&bm, BM_BITS + 1);
    assert(err == BITMAP_ERR_RANGE);

    /* test on NULL returns false */
    assert(bitmap_test(NULL, 0) == false);

    /* count on NULL returns 0 */
    assert(bitmap_count_set(NULL) == 0);
    assert(bitmap_size(NULL) == 0);
}

int main(void) {
    verify_invariants();
    verify_alloc_free();
    verify_set_clear_toggle();
    verify_null_safety();
    return 0;
}
