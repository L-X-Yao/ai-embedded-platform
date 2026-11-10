// SPDX-License-Identifier: MIT
/*
 * test_race_fifo_queue.c -- one producer and one consumer with no lock
 *
 * The header lets one producer and one consumer run at once. Host threads
 * stand in for an ISR and the main loop. Items count up and carry the inverse
 * of their number, and the capacity is prime, so a lost update shows up as an
 * item out of order or torn, or as a stream that stops short because both
 * sides think there is nothing to do.
 *
 * What this cannot show. A pass is evidence, not proof: it covers the
 * interleavings that happened to occur. x86 keeps stores in order, so it says
 * nothing about a weakly ordered core; run on one, this test needs
 * FIFO_QUEUE_BARRIER defined as a real fence. And the code it runs is x86
 * code, not the Cortex-M code a compiler makes from the same source.
 */

#include "test_harness.h"
#include <pthread.h>
#include <stdbool.h>
#include "fifo_queue.h"

#define CAPACITY    13u
#define STREAM      2000000u
#define STALL_SPINS 50000000L /* polls with no item moved: the stream is stuck */

typedef struct {
    uint32_t number;
    uint32_t inverse;
} item_t;

static item_t       g_storage[CAPACITY];
static fifo_queue_t g_queue;
static int          g_stop;

static void *producer(void *arg)
{
    uint32_t sent = 0;

    (void)arg;
    while (sent < STREAM && __atomic_load_n(&g_stop, __ATOMIC_RELAXED) == 0) {
        item_t item = {sent, ~sent};

        if (fifo_queue_push(&g_queue, &item) == FIFO_QUEUE_OK) {
            sent++;
        }
    }
    return NULL;
}

typedef struct {
    uint32_t got;
    long     wrong;
    long     out_of_range;
} result_t;

static result_t consume(bool peek_first)
{
    result_t r = {0, 0, 0};
    long idle = 0;

    while (r.got < STREAM && idle < STALL_SPINS) {
        item_t seen = {0, 0};
        item_t item = {0, 0};

        if (peek_first && fifo_queue_peek(&g_queue, &seen) != FIFO_QUEUE_OK) {
            idle++;
            continue;
        }
        if (fifo_queue_pop(&g_queue, &item) != FIFO_QUEUE_OK) {
            idle++;
            continue;
        }
        idle = 0;

        if (item.number != r.got || item.inverse != ~r.got) {
            r.wrong++;
        }
        if (peek_first &&
            (seen.number != item.number || seen.inverse != item.inverse)) {
            r.wrong++;
        }
        r.got++;
        if (fifo_queue_count(&g_queue) > CAPACITY) {
            r.out_of_range++;
        }
    }
    return r;
}

static int run_stream(bool peek_first, result_t *out)
{
    pthread_t prod;

    if (fifo_queue_init(&g_queue, g_storage, CAPACITY, sizeof(item_t))
        != FIFO_QUEUE_OK) {
        return -1;
    }
    __atomic_store_n(&g_stop, 0, __ATOMIC_RELAXED);
    if (pthread_create(&prod, NULL, producer, NULL) != 0) {
        return -1;
    }
    *out = consume(peek_first);
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
    return pthread_join(prod, NULL);
}

TEST(test_push_and_pop_keep_every_item_in_order)
{
    result_t r;

    ASSERT_EQ(0, run_stream(false, &r));
    ASSERT_EQ(0, r.wrong);
    ASSERT_EQ(0, r.out_of_range);
    ASSERT_EQ(STREAM, r.got);
    ASSERT_TRUE(fifo_queue_is_empty(&g_queue));
    ASSERT_EQ(CAPACITY, fifo_queue_free(&g_queue));
}

TEST(test_peek_then_pop_keeps_every_item_in_order)
{
    result_t r;

    ASSERT_EQ(0, run_stream(true, &r));
    ASSERT_EQ(0, r.wrong);
    ASSERT_EQ(0, r.out_of_range);
    ASSERT_EQ(STREAM, r.got);
    ASSERT_TRUE(fifo_queue_is_empty(&g_queue));
    ASSERT_EQ(CAPACITY, fifo_queue_free(&g_queue));
}

TEST_MAIN("fifo_queue race")
    RUN(test_push_and_pop_keep_every_item_in_order);
    RUN(test_peek_then_pop_keeps_every_item_in_order);
TEST_MAIN_END()
