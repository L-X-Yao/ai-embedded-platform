// SPDX-License-Identifier: MIT
/*
 * stress_fifo_queue.c — long-running stress tests for fifo_queue
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_fifo_queue.c ../fifo_queue.c -o /tmp/stress_fifo_queue -lm
 *
 * Scenarios:
 *   1. 1M single-element push/pop cycles — verify data integrity across wraps
 *   2. Repeated fill-to-capacity then drain — verify count/head/tail consistency
 *   3. Overflow recovery — push when full, verify ERR_FULL and no corruption
 *   4. Struct element integrity — multi-byte elements across 100K wrap cycles
 */

#include "test_harness.h"
#include "fifo_queue.h"
#include <string.h>

#define ITERATIONS    1000000
#define CAPACITY      64

/* ---- tests ---------------------------------------------------------- */

TEST(single_element_million_cycles)
{
    uint32_t storage[CAPACITY];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, CAPACITY, sizeof(uint32_t));

    int ok = 1;
    for (int i = 0; i < ITERATIONS; i++) {
        uint32_t val = (uint32_t)i;
        if (fifo_queue_push(&q, &val) != FIFO_QUEUE_OK) { ok = 0; break; }
        uint32_t out = 0;
        if (fifo_queue_pop(&q, &out) != FIFO_QUEUE_OK) { ok = 0; break; }
        if (out != val) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
    ASSERT_TRUE(fifo_queue_is_empty(&q));
    ASSERT_EQ(0, fifo_queue_count(&q));
}

TEST(fill_drain_cycles)
{
    uint32_t storage[CAPACITY];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, CAPACITY, sizeof(uint32_t));

    int ok = 1;
    const int cycles = 50000;

    for (int c = 0; c < cycles; c++) {
        /* fill to capacity */
        for (uint16_t i = 0; i < CAPACITY; i++) {
            uint32_t val = (uint32_t)(c * CAPACITY + i);
            if (fifo_queue_push(&q, &val) != FIFO_QUEUE_OK) { ok = 0; break; }
        }
        if (!ok) break;
        ASSERT_TRUE(fifo_queue_is_full(&q));
        ASSERT_EQ(CAPACITY, fifo_queue_count(&q));
        ASSERT_EQ(0, fifo_queue_free(&q));

        /* drain and verify order */
        for (uint16_t i = 0; i < CAPACITY; i++) {
            uint32_t out = 0;
            if (fifo_queue_pop(&q, &out) != FIFO_QUEUE_OK) { ok = 0; break; }
            uint32_t expected = (uint32_t)(c * CAPACITY + i);
            if (out != expected) { ok = 0; break; }
        }
        if (!ok) break;
        ASSERT_TRUE(fifo_queue_is_empty(&q));
        ASSERT_EQ(0, fifo_queue_count(&q));
        ASSERT_EQ(CAPACITY, fifo_queue_free(&q));
    }
    ASSERT_TRUE(ok);
}

TEST(overflow_recovery)
{
    uint32_t storage[CAPACITY];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, CAPACITY, sizeof(uint32_t));

    int ok = 1;
    const int cycles = 100000;

    for (int c = 0; c < cycles; c++) {
        /* fill */
        for (uint16_t i = 0; i < CAPACITY; i++) {
            uint32_t val = (uint32_t)i;
            fifo_queue_push(&q, &val);
        }
        /* push when full must fail */
        uint32_t extra = 0xDEAD;
        if (fifo_queue_push(&q, &extra) != FIFO_QUEUE_ERR_FULL) { ok = 0; break; }
        /* count must still be CAPACITY */
        if (fifo_queue_count(&q) != CAPACITY) { ok = 0; break; }

        /* drain — first element must be 0, not 0xDEAD */
        uint32_t first = 0;
        fifo_queue_pop(&q, &first);
        if (first != 0) { ok = 0; break; }

        /* drain rest */
        fifo_queue_clear(&q);
    }
    ASSERT_TRUE(ok);
}

TEST(struct_element_wrap_integrity)
{
    typedef struct {
        uint32_t id;
        uint16_t val;
        uint8_t  tag;
        uint8_t  pad;
    } item_t;

    uint8_t storage[CAPACITY * sizeof(item_t)];
    fifo_queue_t q;
    fifo_queue_init(&q, storage, CAPACITY, sizeof(item_t));

    int ok = 1;
    for (int i = 0; i < 100000; i++) {
        item_t in = { .id = (uint32_t)i, .val = (uint16_t)(i & 0xFFFF),
                      .tag = (uint8_t)(i & 0xFF), .pad = 0 };
        if (fifo_queue_push(&q, &in) != FIFO_QUEUE_OK) { ok = 0; break; }
        item_t out;
        memset(&out, 0, sizeof(out));
        if (fifo_queue_pop(&q, &out) != FIFO_QUEUE_OK) { ok = 0; break; }
        if (out.id != in.id || out.val != in.val || out.tag != in.tag) {
            ok = 0; break;
        }
    }
    ASSERT_TRUE(ok);
    ASSERT_TRUE(fifo_queue_is_empty(&q));
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress fifo_queue")
    RUN(single_element_million_cycles);
    RUN(fill_drain_cycles);
    RUN(overflow_recovery);
    RUN(struct_element_wrap_integrity);
TEST_MAIN_END()
