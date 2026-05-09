/*
 * prop_event_queue.c — property-based tests for event_queue
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_event_queue.c ../event_queue.c -o prop_event_queue
 */

#include "test_harness.h"
#include "property_test.h"
#include "../event_queue.h"
#include <string.h>

#define QUEUE_CAP 32

/* Property: push N events then pop N returns same type+data in FIFO order.
 * Note: event_queue uses head/tail ring with one sentinel slot,
 * so usable capacity = QUEUE_CAP - 1. */
PROPERTY(push_pop_roundtrip, 10000) {
    event_t storage[QUEUE_CAP];
    event_queue_t q;
    event_queue_init(&q, storage, QUEUE_CAP);

    size_t n = (size_t)ARBI_RANGE(seed, 1, QUEUE_CAP - 1);
    uint16_t types[QUEUE_CAP];
    uint32_t datas[QUEUE_CAP];

    for (size_t i = 0; i < n; i++) {
        types[i] = ARBI_UINT16(seed);
        datas[i] = ARBI_UINT32(seed);
        event_queue_err_t err = event_queue_push(&q, types[i], datas[i]);
        PROP_ASSERT_EQ(err, EVENT_QUEUE_OK);
    }

    PROP_ASSERT_EQ(event_queue_count(&q), n);

    for (size_t i = 0; i < n; i++) {
        event_t evt;
        event_queue_err_t err = event_queue_pop(&q, &evt);
        PROP_ASSERT_EQ(err, EVENT_QUEUE_OK);
        PROP_ASSERT_EQ(evt.type, types[i]);
        PROP_ASSERT_EQ(evt.data, datas[i]);
    }

    PROP_ASSERT(event_queue_is_empty(&q));
}

/* Property: count invariant across random ops */
PROPERTY(count_invariant, 10000) {
    event_t storage[16];
    event_queue_t q;
    event_queue_init(&q, storage, 16);

    size_t expected_count = 0;
    unsigned ops = ARBI_RANGE(seed, 0, 100);
    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 2;
        if (op == 0) {
            uint16_t type = ARBI_UINT16(seed);
            uint32_t data = ARBI_UINT32(seed);
            event_queue_err_t err = event_queue_push(&q, type, data);
            if (err == EVENT_QUEUE_OK) {
                expected_count++;
            }
        } else {
            event_t evt;
            event_queue_err_t err = event_queue_pop(&q, &evt);
            if (err == EVENT_QUEUE_OK) {
                expected_count--;
            }
        }
        PROP_ASSERT_EQ(event_queue_count(&q), expected_count);
    }
}

/* Property: push to full queue returns ERR_FULL.
 * Usable slots = capacity - 1 (sentinel slot). */
PROPERTY(full_push_returns_error, 5000) {
    event_t storage[4];
    event_queue_t q;
    event_queue_init(&q, storage, 4);

    /* fill 3 usable slots (capacity - 1) */
    for (int i = 0; i < 3; i++) {
        event_queue_push(&q, (uint16_t)i, (uint32_t)i);
    }
    PROP_ASSERT(event_queue_is_full(&q));

    uint16_t type = ARBI_UINT16(seed);
    uint32_t data = ARBI_UINT32(seed);
    event_queue_err_t err = event_queue_push(&q, type, data);
    PROP_ASSERT_EQ(err, EVENT_QUEUE_ERR_FULL);
}

/* Property: peek returns same as next pop without consuming */
PROPERTY(peek_matches_pop, 5000) {
    event_t storage[8];
    event_queue_t q;
    event_queue_init(&q, storage, 8);

    size_t n = (size_t)ARBI_RANGE(seed, 1, 7); /* usable = capacity - 1 */
    for (size_t i = 0; i < n; i++) {
        event_queue_push(&q, ARBI_UINT16(seed), ARBI_UINT32(seed));
    }

    event_t peeked, popped;
    event_queue_err_t err1 = event_queue_peek(&q, &peeked);
    PROP_ASSERT_EQ(err1, EVENT_QUEUE_OK);

    size_t count_before = event_queue_count(&q);
    event_queue_err_t err2 = event_queue_pop(&q, &popped);
    PROP_ASSERT_EQ(err2, EVENT_QUEUE_OK);

    PROP_ASSERT_EQ(peeked.type, popped.type);
    PROP_ASSERT_EQ(peeked.data, popped.data);
    PROP_ASSERT_EQ(event_queue_count(&q), count_before - 1);
}

/* Property: clear resets to empty state */
PROPERTY(clear_empties, 5000) {
    event_t storage[16];
    event_queue_t q;
    event_queue_init(&q, storage, 16);

    size_t n = (size_t)ARBI_RANGE(seed, 0, 16);
    for (size_t i = 0; i < n; i++) {
        event_queue_push(&q, ARBI_UINT16(seed), ARBI_UINT32(seed));
    }

    event_queue_clear(&q);
    PROP_ASSERT(event_queue_is_empty(&q));
    PROP_ASSERT_EQ(event_queue_count(&q), 0);
}

TEST_MAIN("event_queue properties") {
    RUN(push_pop_roundtrip);
    RUN(count_invariant);
    RUN(full_push_returns_error);
    RUN(peek_matches_pop);
    RUN(clear_empties);
}
TEST_MAIN_END()
