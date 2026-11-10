// SPDX-License-Identifier: MIT
/*
 * test_race_ring_buffer.c -- one producer and one consumer with no lock
 *
 * The header lets one producer and one consumer run at once. Host threads
 * stand in for a UART ISR and the main loop. The bytes count up modulo 256 and
 * the capacity is prime, so a lost update shows up as a wrong byte, or as a
 * stream that stops short because both sides think there is nothing to do.
 *
 * What this cannot show. A pass is evidence, not proof: it covers the
 * interleavings that happened to occur. x86 keeps stores in order, so it says
 * nothing about a weakly ordered core; run on one, this test needs
 * RING_BUFFER_BARRIER defined as a real fence. And the code it runs is x86
 * code, not the Cortex-M code a compiler makes from the same source.
 */

#include "test_harness.h"
#include <pthread.h>
#include <stdbool.h>
#include "ring_buffer.h"

#define CAPACITY    251u
#define STREAM      3000000L
#define STALL_SPINS 50000000L /* polls with no byte moved: the stream is stuck */

static uint8_t       g_storage[CAPACITY];
static ring_buffer_t g_rb;
static int           g_stop;

static void *producer(void *arg)
{
    long sent = 0;

    (void)arg;
    while (sent < STREAM && __atomic_load_n(&g_stop, __ATOMIC_RELAXED) == 0) {
        uint8_t chunk[7];
        uint16_t want = (uint16_t)(1 + sent % 7);

        if (want > STREAM - sent) {
            want = (uint16_t)(STREAM - sent);
        }
        for (uint16_t i = 0; i < want; i++) {
            chunk[i] = (uint8_t)(sent + i);
        }
        sent += ring_buffer_write(&g_rb, chunk, want);
        if (ring_buffer_free(&g_rb) > CAPACITY) {
            break; /* the consumer's count check reports the same fault */
        }
    }
    return NULL;
}

typedef struct {
    long got;
    long wrong;
    long out_of_range;
} result_t;

static result_t consume(bool peek_then_skip)
{
    result_t r = {0, 0, 0};
    long idle = 0;

    while (r.got < STREAM && idle < STALL_SPINS) {
        uint8_t chunk[5];
        uint16_t want = (uint16_t)(1 + r.got % 5);
        uint16_t n;

        if (want > STREAM - r.got) {
            want = (uint16_t)(STREAM - r.got);
        }
        if (peek_then_skip) {
            n = ring_buffer_peek(&g_rb, chunk, want);
            (void)ring_buffer_skip(&g_rb, n);
        } else {
            n = ring_buffer_read(&g_rb, chunk, want);
        }
        idle = (n == 0U) ? idle + 1 : 0;

        for (uint16_t i = 0; i < n; i++) {
            if (chunk[i] != (uint8_t)(r.got + i)) {
                r.wrong++;
            }
        }
        r.got += n;
        if (ring_buffer_count(&g_rb) > CAPACITY) {
            r.out_of_range++;
        }
    }
    return r;
}

static int run_stream(bool peek_then_skip, result_t *out)
{
    pthread_t prod;

    if (ring_buffer_init(&g_rb, g_storage, CAPACITY) != RING_BUFFER_OK) {
        return -1;
    }
    __atomic_store_n(&g_stop, 0, __ATOMIC_RELAXED);
    if (pthread_create(&prod, NULL, producer, NULL) != 0) {
        return -1;
    }
    *out = consume(peek_then_skip);
    __atomic_store_n(&g_stop, 1, __ATOMIC_RELAXED);
    return pthread_join(prod, NULL);
}

TEST(test_writer_and_reader_keep_every_byte_in_order)
{
    result_t r;

    ASSERT_EQ(0, run_stream(false, &r));
    ASSERT_EQ(0, r.wrong);
    ASSERT_EQ(0, r.out_of_range);
    ASSERT_EQ(STREAM, r.got);
    ASSERT_TRUE(ring_buffer_is_empty(&g_rb));
    ASSERT_EQ(CAPACITY, ring_buffer_free(&g_rb));
}

TEST(test_peek_and_skip_keep_every_byte_in_order)
{
    result_t r;

    ASSERT_EQ(0, run_stream(true, &r));
    ASSERT_EQ(0, r.wrong);
    ASSERT_EQ(0, r.out_of_range);
    ASSERT_EQ(STREAM, r.got);
    ASSERT_TRUE(ring_buffer_is_empty(&g_rb));
    ASSERT_EQ(CAPACITY, ring_buffer_free(&g_rb));
}

TEST_MAIN("ring_buffer race")
    RUN(test_writer_and_reader_keep_every_byte_in_order);
    RUN(test_peek_and_skip_keep_every_byte_in_order);
TEST_MAIN_END()
