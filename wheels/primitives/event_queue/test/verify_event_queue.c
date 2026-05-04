/*
 * verify_event_queue.c — CBMC bounded model checking for event_queue
 *
 * Verifies FIFO ordering, capacity tracking, and SPSC invariants.
 * Run with:
 *   cbmc verify_event_queue.c ../event_queue.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 */

#include "../event_queue.h"
#include <stdint.h>
#include <assert.h>

#ifndef UNWIND_BOUND
#define UNWIND_BOUND 50
#endif

#define Q_CAP 8

extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern uint32_t nondet_uint32(void);
extern unsigned  nondet_unsigned(void);

static void check_invariants(const event_queue_t *q) {
    size_t count = event_queue_count(q);
    assert(count <= q->capacity);
    assert(event_queue_is_empty(q) == (count == 0));
    /* sentinel-slot design: usable capacity = N-1 */
    assert(event_queue_is_full(q) == (count == q->capacity - 1));
}

/* Proof 1: invariants hold after any sequence of push/pop/clear */
void verify_invariants(void) {
    event_t storage[Q_CAP];
    event_queue_t q;

    event_queue_err_t err = event_queue_init(&q, storage, Q_CAP);
    assert(err == EVENT_QUEUE_OK);
    check_invariants(&q);

    unsigned ops = nondet_unsigned();
    __CPROVER_assume(ops <= UNWIND_BOUND);

    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = nondet_uint8() % 4;
        switch (op) {
        case 0: {
            uint16_t type = nondet_uint16();
            uint32_t data = nondet_uint32();
            event_queue_push(&q, type, data);
            break;
        }
        case 1: {
            event_t out;
            event_queue_pop(&q, &out);
            break;
        }
        case 2: {
            event_t out;
            event_queue_peek(&q, &out);
            break;
        }
        case 3: {
            event_queue_clear(&q);
            break;
        }
        }
        check_invariants(&q);
    }
}

/* Proof 2: push-then-pop preserves event data and FIFO order */
void verify_fifo_order(void) {
    event_t storage[Q_CAP];
    event_queue_t q;
    event_queue_init(&q, storage, Q_CAP);

    uint16_t types[4];
    uint32_t datas[4];
    for (int i = 0; i < 4; i++) {
        types[i] = nondet_uint16();
        datas[i] = nondet_uint32();
        event_queue_err_t err = event_queue_push(&q, types[i], datas[i]);
        assert(err == EVENT_QUEUE_OK);
    }

    assert(event_queue_count(&q) == 4);

    /* Peek should return first element without consuming */
    event_t peeked;
    event_queue_err_t err = event_queue_peek(&q, &peeked);
    assert(err == EVENT_QUEUE_OK);
    assert(peeked.type == types[0]);
    assert(peeked.data == datas[0]);
    assert(event_queue_count(&q) == 4);

    /* Pop all in FIFO order */
    for (int i = 0; i < 4; i++) {
        event_t out;
        err = event_queue_pop(&q, &out);
        assert(err == EVENT_QUEUE_OK);
        assert(out.type == types[i]);
        assert(out.data == datas[i]);
    }
    assert(event_queue_is_empty(&q));
}

/* Proof 3: full/empty boundary conditions */
void verify_boundary_conditions(void) {
    event_t storage[Q_CAP];
    event_queue_t q;
    event_queue_init(&q, storage, Q_CAP);

    /* Fill to usable capacity (N-1 due to sentinel slot) */
    for (int i = 0; i < Q_CAP - 1; i++) {
        event_queue_err_t err = event_queue_push(&q, (uint16_t)i, (uint32_t)i);
        assert(err == EVENT_QUEUE_OK);
    }
    assert(event_queue_is_full(&q));

    /* Extra push fails */
    event_queue_err_t err = event_queue_push(&q, 0xFF, 0xFF);
    assert(err == EVENT_QUEUE_ERR_FULL);

    /* Drain */
    for (int i = 0; i < Q_CAP - 1; i++) {
        event_t out;
        err = event_queue_pop(&q, &out);
        assert(err == EVENT_QUEUE_OK);
    }
    assert(event_queue_is_empty(&q));

    /* Extra pop fails */
    event_t dummy;
    err = event_queue_pop(&q, &dummy);
    assert(err == EVENT_QUEUE_ERR_EMPTY);
}

/* Proof 4: NULL parameter handling */
void verify_null_safety(void) {
    event_queue_err_t err;

    err = event_queue_init(NULL, NULL, 0);
    assert(err == EVENT_QUEUE_ERR_PARAM);

    event_t storage[4];
    event_queue_t q;
    err = event_queue_init(&q, storage, 4);
    assert(err == EVENT_QUEUE_OK);

    err = event_queue_pop(&q, NULL);
    assert(err != EVENT_QUEUE_OK);

    assert(event_queue_count(NULL) == 0);
    assert(event_queue_is_empty(NULL) == true);
}

int main(void) {
    verify_invariants();
    verify_fifo_order();
    verify_boundary_conditions();
    verify_null_safety();
    return 0;
}
