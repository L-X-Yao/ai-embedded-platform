// SPDX-License-Identifier: MIT
/*
 * stress_event_queue.c — long-running stress tests for event_queue
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_event_queue.c ../event_queue.c -o /tmp/stress_event_queue -lm
 *
 * Scenarios:
 *   1. 1M push/pop cycles — verify event type and data integrity
 *   2. Repeated fill-to-capacity then drain — overflow handling
 *   3. Overflow rejection — push when full, verify no corruption
 *   4. Rapid event type cycling — many different event types over time
 */

#include "test_harness.h"
#include "event_queue.h"

#define ITERATIONS  1000000
#define CAPACITY    128

/* ---- tests ---------------------------------------------------------- */

TEST(single_event_million_cycles)
{
    event_t storage[CAPACITY];
    event_queue_t q;
    event_queue_init(&q, storage, CAPACITY);

    int ok = 1;
    for (int i = 0; i < ITERATIONS; i++) {
        uint16_t type = (uint16_t)(i & 0xFFFF);
        uint32_t data = (uint32_t)i;
        if (event_queue_push(&q, type, data) != EVENT_QUEUE_OK) { ok = 0; break; }

        event_t ev;
        if (event_queue_pop(&q, &ev) != EVENT_QUEUE_OK) { ok = 0; break; }
        if (ev.type != type || ev.data != data) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
    ASSERT_TRUE(event_queue_is_empty(&q));
}

TEST(fill_drain_cycles)
{
    event_t storage[CAPACITY];
    event_queue_t q;
    event_queue_init(&q, storage, CAPACITY);

    int ok = 1;
    /* capacity-1 because circular buffer wastes one slot */
    const size_t usable = CAPACITY - 1;
    const int cycles = 20000;

    for (int c = 0; c < cycles; c++) {
        /* fill to usable capacity */
        for (size_t i = 0; i < usable; i++) {
            uint16_t type = (uint16_t)(i + 1);
            uint32_t data = (uint32_t)(c * usable + i);
            if (event_queue_push(&q, type, data) != EVENT_QUEUE_OK) { ok = 0; break; }
        }
        if (!ok) break;
        ASSERT_TRUE(event_queue_is_full(&q));

        /* drain and verify FIFO order */
        for (size_t i = 0; i < usable; i++) {
            event_t ev;
            if (event_queue_pop(&q, &ev) != EVENT_QUEUE_OK) { ok = 0; break; }
            uint32_t expected_data = (uint32_t)(c * usable + i);
            if (ev.data != expected_data) { ok = 0; break; }
        }
        if (!ok) break;
        ASSERT_TRUE(event_queue_is_empty(&q));
    }
    ASSERT_TRUE(ok);
}

TEST(overflow_rejection)
{
    event_t storage[CAPACITY];
    event_queue_t q;
    event_queue_init(&q, storage, CAPACITY);

    int ok = 1;
    const int cycles = 100000;

    for (int c = 0; c < cycles; c++) {
        /* fill */
        while (!event_queue_is_full(&q)) {
            event_queue_push(&q, 0x01, (uint32_t)c);
        }
        /* overflow must fail */
        if (event_queue_push(&q, 0xFF, 0xDEAD) != EVENT_QUEUE_ERR_FULL) {
            ok = 0; break;
        }
        /* first pop must NOT be the overflow event */
        event_t ev;
        event_queue_pop(&q, &ev);
        if (ev.type != 0x01) { ok = 0; break; }

        /* drain rest */
        event_queue_clear(&q);
    }
    ASSERT_TRUE(ok);
}

TEST(rapid_type_cycling)
{
    event_t storage[CAPACITY];
    event_queue_t q;
    event_queue_init(&q, storage, CAPACITY);

    int ok = 1;
    /* push events with varying types, verify they come back in order */
    for (int i = 0; i < 500000; i++) {
        uint16_t type = (uint16_t)((i * 7 + 3) & 0xFFFF); /* pseudo-random */
        uint32_t data = (uint32_t)i;
        if (event_queue_push(&q, type, data) != EVENT_QUEUE_OK) { ok = 0; break; }

        event_t ev;
        if (event_queue_pop(&q, &ev) != EVENT_QUEUE_OK) { ok = 0; break; }
        if (ev.type != type || ev.data != data) { ok = 0; break; }
    }
    ASSERT_TRUE(ok);
    ASSERT_EQ(0, event_queue_count(&q));
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress event_queue")
    RUN(single_event_million_cycles);
    RUN(fill_drain_cycles);
    RUN(overflow_rejection);
    RUN(rapid_type_cycling);
TEST_MAIN_END()
