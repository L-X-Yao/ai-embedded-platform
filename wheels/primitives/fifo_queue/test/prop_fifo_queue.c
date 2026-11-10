// SPDX-License-Identifier: MIT
/*
 * prop_fifo_queue.c — property-based tests for fifo_queue
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_fifo_queue.c ../fifo_queue.c -o prop_fifo_queue
 */

#include "test_harness.h"
#include "property_test.h"
#include "../fifo_queue.h"
#include <string.h>

#define QUEUE_CAP 32

/* Property: push N items then pop N returns same data in FIFO order */
PROPERTY(push_pop_roundtrip, 10000) {
    uint32_t storage[QUEUE_CAP];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, QUEUE_CAP, sizeof(uint32_t));

    uint16_t n = (uint16_t)ARBI_RANGE(seed, 1, QUEUE_CAP);
    uint32_t values[QUEUE_CAP];
    for (uint16_t i = 0; i < n; i++) {
        values[i] = ARBI_UINT32(seed);
        fifo_queue_err_t err = fifo_queue_push(&q, &values[i]);
        PROP_ASSERT_EQ(err, FIFO_QUEUE_OK);
    }

    PROP_ASSERT_EQ(fifo_queue_count(&q), n);

    for (uint16_t i = 0; i < n; i++) {
        uint32_t out;
        fifo_queue_err_t err = fifo_queue_pop(&q, &out);
        PROP_ASSERT_EQ(err, FIFO_QUEUE_OK);
        PROP_ASSERT_EQ(out, values[i]);
    }

    PROP_ASSERT(fifo_queue_is_empty(&q));
}

/* Property: count + free always equals capacity */
PROPERTY(count_plus_free_eq_capacity, 10000) {
    uint8_t storage[QUEUE_CAP * sizeof(uint16_t)];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, QUEUE_CAP, sizeof(uint16_t));

    unsigned ops = ARBI_RANGE(seed, 0, 100);
    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 2;
        if (op == 0) {
            uint16_t val = ARBI_UINT16(seed);
            fifo_queue_push(&q, &val);
        } else {
            uint16_t val;
            fifo_queue_pop(&q, &val);
        }
        PROP_ASSERT_EQ(fifo_queue_count(&q) + fifo_queue_free(&q), QUEUE_CAP);
    }
}

/* Property: push to full queue returns ERR_FULL, does not corrupt */
PROPERTY(full_push_safe, 5000) {
    uint8_t storage[8 * sizeof(uint8_t)];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, 8, sizeof(uint8_t));

    uint8_t fill[8];
    ARBI_FILL(seed, fill, 8);
    for (int i = 0; i < 8; i++) {
        fifo_queue_push(&q, &fill[i]);
    }
    PROP_ASSERT(fifo_queue_is_full(&q));

    uint8_t extra = ARBI_UINT8(seed);
    fifo_queue_err_t err = fifo_queue_push(&q, &extra);
    PROP_ASSERT_EQ(err, FIFO_QUEUE_ERR_FULL);
    PROP_ASSERT_EQ(fifo_queue_count(&q), 8);

    /* verify data integrity */
    for (int i = 0; i < 8; i++) {
        uint8_t out;
        fifo_queue_pop(&q, &out);
        PROP_ASSERT_EQ(out, fill[i]);
    }
}

/* Property: pop from empty queue returns ERR_EMPTY */
PROPERTY(empty_pop_safe, 5000) {
    uint32_t storage[4];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, 4, sizeof(uint32_t));

    PROP_ASSERT(fifo_queue_is_empty(&q));

    uint32_t out;
    fifo_queue_err_t err = fifo_queue_pop(&q, &out);
    PROP_ASSERT_EQ(err, FIFO_QUEUE_ERR_EMPTY);
}

/* Property: random mix of push/pop never corrupts internal state */
PROPERTY(random_ops_no_corruption, 10000) {
    uint32_t storage[16];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, 16, sizeof(uint32_t));

    unsigned ops = ARBI_RANGE(seed, 0, 200);
    unsigned stored = 0;
    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 3;
        if (op <= 1) { /* push biased */
            uint32_t val = ARBI_UINT32(seed);
            if (fifo_queue_push(&q, &val) == FIFO_QUEUE_OK) {
                stored++;
            }
        } else {
            uint32_t val;
            if (fifo_queue_pop(&q, &val) == FIFO_QUEUE_OK) {
                stored--;
            }
        }

        PROP_ASSERT(stored <= 16);
        PROP_ASSERT(fifo_queue_count(&q) == stored);
        PROP_ASSERT(q.head < 2U * q.capacity);
        PROP_ASSERT(q.tail < 2U * q.capacity);
    }
}

TEST_MAIN("fifo_queue properties") {
    RUN(push_pop_roundtrip);
    RUN(count_plus_free_eq_capacity);
    RUN(full_push_safe);
    RUN(empty_pop_safe);
    RUN(random_ops_no_corruption);
}
TEST_MAIN_END()
