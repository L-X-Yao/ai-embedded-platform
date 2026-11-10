// SPDX-License-Identifier: MIT
/*
 * verify_ring_buffer.c — CBMC bounded model checking for ring_buffer
 *
 * Verifies structural invariants hold for ALL possible operation sequences
 * up to OPS_BOUND steps. Run with:
 *   cbmc verify_ring_buffer.c ../ring_buffer.c --unwind 51 \
 *        --bounds-check --pointer-check --unsigned-overflow-check
 *
 * If CBMC is not installed, this file is skipped by the test suite.
 */

#include "../ring_buffer.h"
#include <stddef.h>
#include <stdint.h>
#include <assert.h>


#define BUF_CAP 8

/*
 * The state is head and tail, 0 .. 2*BUF_CAP-1. At up to 4 bytes an operation,
 * all 144 states are reachable and the farthest takes 10 operations, so 12 tries
 * every operation from every state.
 */
#ifndef OPS_BOUND
#define OPS_BOUND 12
#endif

/* CBMC built-in: returns arbitrary value of given type */
extern uint8_t  nondet_uint8(void);
extern uint16_t nondet_uint16(void);
extern unsigned  nondet_unsigned(void);

/* The level comes straight from the raw indexes, so the clamp in the wheel
 * cannot hide an overrun. */
static void check_invariants(const ring_buffer_t *rb) {
    uint32_t span = 2U * rb->capacity;
    assert(rb->head < span);
    assert(rb->tail < span);
    uint32_t level = (rb->head >= rb->tail) ? rb->head - rb->tail
                                            : rb->head + span - rb->tail;
    assert(level <= rb->capacity);
    assert(ring_buffer_count(rb) == level);
    assert(ring_buffer_free(rb) == rb->capacity - level);
    assert(ring_buffer_is_empty(rb) == (level == 0));
    assert(ring_buffer_is_full(rb) == (level == rb->capacity));
}

/* Proof 1: invariants hold after any sequence of operations */
void verify_invariants_all_ops(void) {
    uint8_t storage[BUF_CAP];
    ring_buffer_t rb;

    ring_buffer_err_t err = ring_buffer_init(&rb, storage, BUF_CAP);
    assert(err == RING_BUFFER_OK);
    check_invariants(&rb);

    /* A constant bound: the checks after each operation cover every shorter
     * sequence too. */
    for (unsigned i = 0; i < OPS_BOUND; i++) {
        uint8_t op = nondet_uint8() % 5;
        switch (op) {
        case 0: {
            uint8_t data = nondet_uint8();
            ring_buffer_write(&rb, &data, 1);
            break;
        }
        case 1: {
            uint8_t data;
            ring_buffer_read(&rb, &data, 1);
            break;
        }
        case 2: {
            uint8_t data[4];
            uint16_t n = (uint16_t)(nondet_uint8() % 5);
            ring_buffer_write(&rb, data, n);
            break;
        }
        case 3: {
            uint8_t data[4];
            uint16_t n = (uint16_t)(nondet_uint8() % 5);
            ring_buffer_read(&rb, data, n);
            break;
        }
        case 4: {
            uint16_t n = (uint16_t)(nondet_uint8() % 5);
            ring_buffer_skip(&rb, n);
            break;
        }
        }
        check_invariants(&rb);
    }
}

/* Proof 2: write-then-read returns original data */
void verify_write_read_roundtrip(void) {
    uint8_t storage[BUF_CAP];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_CAP);

    uint8_t input[4];
    for (int i = 0; i < 4; i++) input[i] = nondet_uint8();

    uint16_t written = ring_buffer_write(&rb, input, 4);
    assert(written == 4);

    uint8_t output[4] = {0};
    uint16_t read = ring_buffer_read(&rb, output, 4);
    assert(read == 4);

    for (int i = 0; i < 4; i++) {
        assert(output[i] == input[i]);
    }
    assert(ring_buffer_is_empty(&rb));
}

/* Proof 3: full buffer rejects writes, empty buffer rejects reads */
void verify_boundary_conditions(void) {
    uint8_t storage[BUF_CAP];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_CAP);

    /* Fill to capacity */
    uint8_t fill[BUF_CAP];
    for (int i = 0; i < BUF_CAP; i++) fill[i] = nondet_uint8();
    uint16_t w = ring_buffer_write(&rb, fill, BUF_CAP);
    assert(w == BUF_CAP);
    assert(ring_buffer_is_full(&rb));

    /* Extra write returns 0 */
    uint8_t extra = 0xFF;
    w = ring_buffer_write(&rb, &extra, 1);
    assert(w == 0);

    /* Drain completely */
    uint8_t drain[BUF_CAP];
    uint16_t r = ring_buffer_read(&rb, drain, BUF_CAP);
    assert(r == BUF_CAP);
    assert(ring_buffer_is_empty(&rb));

    /* Extra read returns 0 */
    uint8_t dummy;
    r = ring_buffer_read(&rb, &dummy, 1);
    assert(r == 0);
}

/* Proof 4: NULL parameter handling */
void verify_null_safety(void) {
    ring_buffer_err_t err;

    err = ring_buffer_init(NULL, NULL, 0);
    assert(err != RING_BUFFER_OK);

    uint8_t storage[4];
    ring_buffer_t rb;
    err = ring_buffer_init(&rb, storage, 4);
    assert(err == RING_BUFFER_OK);

    uint16_t w = ring_buffer_write(&rb, NULL, 5);
    assert(w == 0);

    uint16_t r = ring_buffer_read(&rb, NULL, 5);
    assert(r == 0);
}

int main(void) {
    verify_invariants_all_ops();
    verify_write_read_roundtrip();
    verify_boundary_conditions();
    verify_null_safety();
    return 0;
}
