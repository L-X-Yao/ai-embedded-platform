// SPDX-License-Identifier: MIT
/*
 * verify_fifo_queue.c — CBMC bounded model checking for fifo_queue
 *
 * Verifies structural invariants hold for ALL possible operation sequences
 * up to UNWIND_BOUND steps. Run with:
 *   cbmc verify_fifo_queue.c ../fifo_queue.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../fifo_queue.h"
#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

#define Q_CAP 8

extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern unsigned  nondet_unsigned(void);

typedef struct {
    uint8_t a;
    uint8_t b;
} test_elem_t;

/* stored comes from the return values, so it checks the indexes independently. */
static void check_invariants(const fifo_queue_t *q, unsigned stored) {
    assert(q->head < 2U * q->capacity);
    assert(q->tail < 2U * q->capacity);
    assert(stored <= q->capacity);
    assert(fifo_queue_count(q) == stored);
    assert(fifo_queue_free(q) == q->capacity - stored);
    assert(fifo_queue_is_empty(q) == (stored == 0));
    assert(fifo_queue_is_full(q) == (stored == q->capacity));
}

/* Proof 1: invariants hold after any sequence of push/pop/peek/clear */
void verify_invariants(void) {
    test_elem_t storage[Q_CAP];
    fifo_queue_t q;

    fifo_queue_err_t err = fifo_queue_init(&q, storage, Q_CAP,
                                           sizeof(test_elem_t));
    assert(err == FIFO_QUEUE_OK);
    unsigned stored = 0;
    check_invariants(&q, stored);

    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= UNWIND_BOUND);

    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = nondet_uint8() % 4;
        switch (op) {
        case 0: {
            test_elem_t item = { nondet_uint8(), nondet_uint8() };
            if (fifo_queue_push(&q, &item) == FIFO_QUEUE_OK) { stored++; }
            break;
        }
        case 1: {
            test_elem_t item;
            if (fifo_queue_pop(&q, &item) == FIFO_QUEUE_OK) { stored--; }
            break;
        }
        case 2: {
            test_elem_t item;
            fifo_queue_peek(&q, &item);
            break;
        }
        case 3: {
            fifo_queue_clear(&q);
            stored = 0;
            break;
        }
        }
        check_invariants(&q, stored);
    }
}

/* Proof 2: push-then-pop returns original data (FIFO order) */
void verify_fifo_order(void) {
    test_elem_t storage[Q_CAP];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, Q_CAP, sizeof(test_elem_t));

    test_elem_t in[4];
    for (int i = 0; i < 4; i++) {
        in[i].a = nondet_uint8();
        in[i].b = nondet_uint8();
        fifo_queue_err_t err = fifo_queue_push(&q, &in[i]);
        assert(err == FIFO_QUEUE_OK);
    }
    assert(fifo_queue_count(&q) == 4);

    for (int i = 0; i < 4; i++) {
        test_elem_t out;
        fifo_queue_err_t err = fifo_queue_pop(&q, &out);
        assert(err == FIFO_QUEUE_OK);
        assert(out.a == in[i].a);
        assert(out.b == in[i].b);
    }
    assert(fifo_queue_is_empty(&q));
}

/* Proof 3: full queue rejects push, empty queue rejects pop */
void verify_boundary_conditions(void) {
    test_elem_t storage[Q_CAP];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, Q_CAP, sizeof(test_elem_t));

    /* Fill to capacity */
    for (int i = 0; i < Q_CAP; i++) {
        test_elem_t item = { (uint8_t)i, 0 };
        fifo_queue_err_t err = fifo_queue_push(&q, &item);
        assert(err == FIFO_QUEUE_OK);
    }
    assert(fifo_queue_is_full(&q));

    /* Extra push must fail */
    test_elem_t extra = { 0xFF, 0xFF };
    fifo_queue_err_t err = fifo_queue_push(&q, &extra);
    assert(err == FIFO_QUEUE_ERR_FULL);

    /* Drain completely */
    for (int i = 0; i < Q_CAP; i++) {
        test_elem_t out;
        err = fifo_queue_pop(&q, &out);
        assert(err == FIFO_QUEUE_OK);
    }
    assert(fifo_queue_is_empty(&q));

    /* Extra pop must fail */
    test_elem_t dummy;
    err = fifo_queue_pop(&q, &dummy);
    assert(err == FIFO_QUEUE_ERR_EMPTY);
}

/* Proof 4: NULL parameter handling */
void verify_null_safety(void) {
    fifo_queue_err_t err;

    err = fifo_queue_init(NULL, NULL, 0, 0);
    assert(err == FIFO_QUEUE_ERR_PARAM);

    test_elem_t storage[4];
    fifo_queue_t q;
    err = fifo_queue_init(&q, storage, 4, sizeof(test_elem_t));
    assert(err == FIFO_QUEUE_OK);

    err = fifo_queue_push(&q, NULL);
    assert(err == FIFO_QUEUE_ERR_PARAM);

    err = fifo_queue_pop(&q, NULL);
    assert(err != FIFO_QUEUE_OK);

    assert(fifo_queue_count(NULL) == 0);
    assert(fifo_queue_is_empty(NULL) == true);
}

int main(void) {
    verify_invariants();
    verify_fifo_order();
    verify_boundary_conditions();
    verify_null_safety();
    return 0;
}
