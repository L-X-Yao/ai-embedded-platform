// SPDX-License-Identifier: MIT
/*
 * test_race_event_queue.c -- event_queue_count beside a live producer and consumer
 *
 * The header lets any context call count while an ISR pushes and the main loop
 * pops. Host threads stand in for those two here. A count built from one read
 * of head and one of tail is always below capacity, whatever they do. Reading
 * either twice lets a wrap land between the reads, and the count goes huge.
 */

#include "test_harness.h"
#include <pthread.h>
#include "event_queue.h"

#define CAPACITY 8u
#define CHECKS   2000000L

static event_t       g_storage[CAPACITY];
static event_queue_t g_queue;
static int           g_stop;

static void *producer(void *arg)
{
    (void)arg;
    while (__atomic_load_n(&g_stop, __ATOMIC_RELAXED) == 0) {
        (void)event_queue_push(&g_queue, 1u, 0u);
    }
    return NULL;
}

static void *consumer(void *arg)
{
    event_t ev;
    (void)arg;
    while (__atomic_load_n(&g_stop, __ATOMIC_RELAXED) == 0) {
        (void)event_queue_pop(&g_queue, &ev);
    }
    return NULL;
}

TEST(test_count_stays_below_capacity)
{
    pthread_t prod;
    pthread_t cons;
    long over = 0;

    ASSERT_EQ(EVENT_QUEUE_OK, event_queue_init(&g_queue, g_storage, CAPACITY));
    ASSERT_EQ(0, pthread_create(&prod, NULL, producer, NULL));
    ASSERT_EQ(0, pthread_create(&cons, NULL, consumer, NULL));

    for (long i = 0; i < CHECKS; i++) {
        if (event_queue_count(&g_queue) >= CAPACITY) {
            over++;
        }
    }

    __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
    ASSERT_EQ(0, pthread_join(prod, NULL));
    ASSERT_EQ(0, pthread_join(cons, NULL));
    ASSERT_EQ(0, over);
}

TEST_MAIN("event_queue race")
    RUN(test_count_stays_below_capacity);
TEST_MAIN_END()
