// SPDX-License-Identifier: MIT
/*
 * test_race_double_buffer.c -- a swapper and a reader with no lock
 *
 * The header lets one context swap while another polls. Host threads stand in
 * for a DMA-complete ISR and the main loop. This target builds with -O2 -flto,
 * so the compiler sees through the calls: a field that is not volatile gets
 * read once, and the poll never sees the swap.
 *
 * What this cannot show. A pass is evidence, not proof: it covers the
 * interleavings that happened to occur. x86 keeps stores in order, so it says
 * nothing about a weakly ordered core; run on one, this test needs
 * DOUBLE_BUFFER_BARRIER defined as a real fence. And it hands each frame over
 * with an acknowledgement, so it does not cover a swap landing during a read,
 * which the header leaves to the caller.
 */

#include "test_harness.h"
#include <pthread.h>
#include "double_buffer.h"

#define FRAME_SIZE  64u
#define FRAMES      200000L
#define POLL_LIMIT  2000000000L /* polls before a wait counts as stuck */

static uint8_t         g_a[FRAME_SIZE];
static uint8_t         g_b[FRAME_SIZE];
static double_buffer_t g_db;
static long            g_acked;
static int             g_stop;

static void *swap_once(void *arg)
{
    (void)arg;
    (void)double_buffer_swap(&g_db);
    return NULL;
}

/* Fill the write buffer, then swap. Each frame waits for the last one's ack. */
static void *producer(void *arg)
{
    (void)arg;
    for (long frame = 1; frame <= FRAMES; frame++) {
        uint8_t *dst = double_buffer_get_write(&g_db);

        for (unsigned i = 0; i < FRAME_SIZE; i++) {
            dst[i] = (uint8_t)(frame + (long)i);
        }
        (void)double_buffer_swap(&g_db);

        while (__atomic_load_n(&g_acked, __ATOMIC_ACQUIRE) != frame) {
            if (__atomic_load_n(&g_stop, __ATOMIC_RELAXED) != 0) {
                return NULL;
            }
        }
    }
    return NULL;
}

TEST(test_poll_sees_a_swap_from_another_context)
{
    pthread_t swapper;
    long polls = 0;

    ASSERT_EQ(DOUBLE_BUFFER_OK, double_buffer_init(&g_db, g_a, g_b, FRAME_SIZE));
    ASSERT_EQ(0, pthread_create(&swapper, NULL, swap_once, NULL));

    while (!double_buffer_is_ready(&g_db) && polls < POLL_LIMIT) {
        polls++;
    }
    bool seen = double_buffer_is_ready(&g_db);

    ASSERT_EQ(0, pthread_join(swapper, NULL));
    ASSERT_TRUE(polls < POLL_LIMIT);
    ASSERT_TRUE(seen);
}

TEST(test_each_frame_is_whole_when_the_read_buffer_changes)
{
    pthread_t prod;
    const uint8_t *last;
    long wrong = 0;
    long frame = 1;

    ASSERT_EQ(DOUBLE_BUFFER_OK, double_buffer_init(&g_db, g_a, g_b, FRAME_SIZE));
    last = double_buffer_get_read(&g_db);
    __atomic_store_n(&g_acked, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_stop, 0, __ATOMIC_RELAXED);
    ASSERT_EQ(0, pthread_create(&prod, NULL, producer, NULL));

    for (; frame <= FRAMES; frame++) {
        const uint8_t *now = double_buffer_get_read(&g_db);
        long polls = 0;

        while (now == last && polls < POLL_LIMIT) {
            now = double_buffer_get_read(&g_db);
            polls++;
        }
        if (now == last) {
            break;
        }
        for (unsigned i = 0; i < FRAME_SIZE; i++) {
            if (now[i] != (uint8_t)(frame + (long)i)) {
                wrong++;
            }
        }
        last = now;
        __atomic_store_n(&g_acked, frame, __ATOMIC_RELEASE);
    }

    __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
    ASSERT_EQ(0, pthread_join(prod, NULL));
    ASSERT_EQ(0, wrong);
    ASSERT_EQ(FRAMES + 1, frame);
}

TEST_MAIN("double_buffer race")
    RUN(test_poll_sees_a_swap_from_another_context);
    RUN(test_each_frame_is_whole_when_the_read_buffer_changes);
TEST_MAIN_END()
