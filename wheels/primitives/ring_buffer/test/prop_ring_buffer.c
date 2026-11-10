// SPDX-License-Identifier: MIT
/*
 * prop_ring_buffer.c — property-based tests for ring_buffer
 *
 * Requires: property_test.h from tooling/harness/
 * Build: add to test/CMakeLists.txt or compile standalone:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_ring_buffer.c ../ring_buffer.c -o prop_ring_buffer
 */

#include "test_harness.h"
#include "property_test.h"
#include "../ring_buffer.h"
#include <string.h>

#define BUF_CAP 64

/* Property: write N bytes then read N bytes returns original data */
PROPERTY(write_read_roundtrip, 10000) {
    uint8_t storage[BUF_CAP];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_CAP);

    uint8_t input[BUF_CAP];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 1, BUF_CAP);
    ARBI_FILL(seed, input, len);

    uint16_t written = ring_buffer_write(&rb, input, len);
    PROP_ASSERT_EQ(written, len);

    uint8_t output[BUF_CAP];
    uint16_t read = ring_buffer_read(&rb, output, len);
    PROP_ASSERT_EQ(read, len);
    PROP_ASSERT_MEM_EQ(output, input, len);
    PROP_ASSERT(ring_buffer_is_empty(&rb));
}

/* Property: count + free always equals capacity */
PROPERTY(count_plus_free_eq_capacity, 10000) {
    uint8_t storage[BUF_CAP];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_CAP);

    unsigned ops = ARBI_RANGE(seed, 0, 100);
    for (unsigned i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 3;
        if (op == 0) {
            uint8_t d = ARBI_UINT8(seed);
            ring_buffer_write(&rb, &d, 1);
        } else if (op == 1) {
            uint8_t d;
            ring_buffer_read(&rb, &d, 1);
        } else {
            ring_buffer_skip(&rb, 1);
        }
        PROP_ASSERT_EQ(ring_buffer_count(&rb) + ring_buffer_free(&rb), BUF_CAP);
    }
}

/* Property: buffer never overflows regardless of operation sequence */
PROPERTY(no_overflow, 50000) {
    uint8_t storage[BUF_CAP];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_CAP);

    unsigned ops = ARBI_RANGE(seed, 0, 200);
    unsigned stored = 0;
    for (unsigned i = 0; i < ops; i++) {
        uint8_t data[8];
        uint16_t n = (uint16_t)ARBI_RANGE(seed, 0, 8);
        ARBI_FILL(seed, data, n);

        if (ARBI_UINT8(seed) % 2 == 0) {
            stored += ring_buffer_write(&rb, data, n);
        } else {
            stored -= ring_buffer_read(&rb, data, n);
        }

        PROP_ASSERT(rb.head < 2U * rb.capacity);
        PROP_ASSERT(rb.tail < 2U * rb.capacity);
        PROP_ASSERT(stored <= rb.capacity);
        PROP_ASSERT(ring_buffer_count(&rb) == stored);
    }
}

/* Property: peek does not consume data */
PROPERTY(peek_nonconsuming, 5000) {
    uint8_t storage[BUF_CAP];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_CAP);

    uint8_t input[16];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 1, 16);
    ARBI_FILL(seed, input, len);
    ring_buffer_write(&rb, input, len);

    uint16_t count_before = ring_buffer_count(&rb);
    uint8_t peeked[16];
    ring_buffer_peek(&rb, peeked, len);
    PROP_ASSERT_EQ(ring_buffer_count(&rb), count_before);
    PROP_ASSERT_MEM_EQ(peeked, input, len);
}

/* Property: write to full buffer returns 0, doesn't corrupt */
PROPERTY(full_write_safe, 5000) {
    uint8_t storage[8];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 8);

    uint8_t fill[8];
    ARBI_FILL(seed, fill, 8);
    ring_buffer_write(&rb, fill, 8);
    PROP_ASSERT(ring_buffer_is_full(&rb));

    uint8_t extra = ARBI_UINT8(seed);
    uint16_t w = ring_buffer_write(&rb, &extra, 1);
    PROP_ASSERT_EQ(w, 0);
    PROP_ASSERT_EQ(ring_buffer_count(&rb), 8);

    uint8_t readback[8];
    ring_buffer_read(&rb, readback, 8);
    PROP_ASSERT_MEM_EQ(readback, fill, 8);
}

TEST_MAIN("ring_buffer properties") {
    RUN(write_read_roundtrip);
    RUN(count_plus_free_eq_capacity);
    RUN(no_overflow);
    RUN(peek_nonconsuming);
    RUN(full_write_safe);
}
TEST_MAIN_END()
