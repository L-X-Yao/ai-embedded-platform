// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "ring_buffer.h"

/* ── init ───────────────────────────────────────────────────────── */

TEST(test_init_valid)
{
    uint8_t storage[16];
    ring_buffer_t rb;
    ASSERT_EQ(ring_buffer_init(&rb, storage, 16), RING_BUFFER_OK);
    ASSERT_EQ(ring_buffer_count(&rb), 0);
    ASSERT_EQ(ring_buffer_free(&rb), 16);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
    ASSERT_FALSE(ring_buffer_is_full(&rb));
}

TEST(test_init_null_rb)
{
    uint8_t storage[16];
    ASSERT_EQ(ring_buffer_init(NULL, storage, 16), RING_BUFFER_ERR_PARAM);
}

TEST(test_init_null_buf)
{
    ring_buffer_t rb;
    ASSERT_EQ(ring_buffer_init(&rb, NULL, 16), RING_BUFFER_ERR_PARAM);
}

TEST(test_init_zero_capacity)
{
    uint8_t storage[1];
    ring_buffer_t rb;
    ASSERT_EQ(ring_buffer_init(&rb, storage, 0), RING_BUFFER_ERR_PARAM);
}

/* ── single byte write/read ─────────────────────────────────────── */

TEST(test_write_read_single)
{
    uint8_t storage[8];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 8);

    uint8_t w = 0xAB;
    ASSERT_EQ(ring_buffer_write(&rb, &w, 1), 1);
    ASSERT_EQ(ring_buffer_count(&rb), 1);

    uint8_t r = 0;
    ASSERT_EQ(ring_buffer_read(&rb, &r, 1), 1);
    ASSERT_EQ(r, 0xAB);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
}

/* ── fill exactly ───────────────────────────────────────────────── */

TEST(test_write_fills_exactly)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);

    uint8_t data[] = {1, 2, 3, 4};
    ASSERT_EQ(ring_buffer_write(&rb, data, 4), 4);
    ASSERT_TRUE(ring_buffer_is_full(&rb));
    ASSERT_EQ(ring_buffer_free(&rb), 0);
    ASSERT_EQ(ring_buffer_count(&rb), 4);
}

/* ── write when full ────────────────────────────────────────────── */

TEST(test_write_when_full)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);

    uint8_t data[] = {1, 2, 3, 4};
    ring_buffer_write(&rb, data, 4);

    uint8_t extra = 0xFF;
    ASSERT_EQ(ring_buffer_write(&rb, &extra, 1), 0);
    ASSERT_TRUE(ring_buffer_is_full(&rb));
}

/* ── read when empty ────────────────────────────────────────────── */

TEST(test_read_when_empty)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);

    uint8_t r = 0xFF;
    ASSERT_EQ(ring_buffer_read(&rb, &r, 1), 0);
    ASSERT_EQ(r, 0xFF); /* unchanged */
}

/* ── wraparound ─────────────────────────────────────────────────── */

TEST(test_wraparound)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);

    /* Write 3, read 3 → tail advances past midpoint */
    uint8_t w1[] = {10, 20, 30};
    ring_buffer_write(&rb, w1, 3);
    uint8_t tmp[3];
    ring_buffer_read(&rb, tmp, 3);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));

    /* Now write 4 bytes — wraps around */
    uint8_t w2[] = {40, 50, 60, 70};
    ASSERT_EQ(ring_buffer_write(&rb, w2, 4), 4);
    ASSERT_TRUE(ring_buffer_is_full(&rb));

    uint8_t out[4];
    ASSERT_EQ(ring_buffer_read(&rb, out, 4), 4);
    ASSERT_EQ(out[0], 40);
    ASSERT_EQ(out[1], 50);
    ASSERT_EQ(out[2], 60);
    ASSERT_EQ(out[3], 70);
}

/* ── partial write ──────────────────────────────────────────────── */

TEST(test_partial_write)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);

    uint8_t data[] = {1, 2, 3, 4, 5, 6};
    ASSERT_EQ(ring_buffer_write(&rb, data, 6), 4); /* only 4 fit */
    ASSERT_TRUE(ring_buffer_is_full(&rb));

    uint8_t out[4];
    ring_buffer_read(&rb, out, 4);
    ASSERT_EQ(out[0], 1);
    ASSERT_EQ(out[3], 4);
}

/* ── peek ───────────────────────────────────────────────────────── */

TEST(test_peek_no_consume)
{
    uint8_t storage[8];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 8);

    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    ring_buffer_write(&rb, data, 3);

    uint8_t peek_buf[3];
    ASSERT_EQ(ring_buffer_peek(&rb, peek_buf, 3), 3);
    ASSERT_EQ(peek_buf[0], 0xAA);
    ASSERT_EQ(peek_buf[2], 0xCC);

    /* count unchanged after peek */
    ASSERT_EQ(ring_buffer_count(&rb), 3);

    /* read still yields same data */
    uint8_t read_buf[3];
    ring_buffer_read(&rb, read_buf, 3);
    ASSERT_EQ(read_buf[0], 0xAA);
}

/* ── skip ───────────────────────────────────────────────────────── */

TEST(test_skip)
{
    uint8_t storage[8];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 8);

    uint8_t data[] = {1, 2, 3, 4, 5};
    ring_buffer_write(&rb, data, 5);

    ASSERT_EQ(ring_buffer_skip(&rb, 2), 2);
    ASSERT_EQ(ring_buffer_count(&rb), 3);

    uint8_t out[3];
    ring_buffer_read(&rb, out, 3);
    ASSERT_EQ(out[0], 3);
    ASSERT_EQ(out[1], 4);
    ASSERT_EQ(out[2], 5);
}

/* ── count/free/empty/full consistency ──────────────────────────── */

TEST(test_count_free_consistency)
{
    uint8_t storage[8];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 8);

    for (uint16_t i = 0; i < 8; i++) {
        uint8_t b = (uint8_t)i;
        ring_buffer_write(&rb, &b, 1);
        ASSERT_EQ(ring_buffer_count(&rb) + ring_buffer_free(&rb), 8);
    }
    ASSERT_TRUE(ring_buffer_is_full(&rb));
    ASSERT_FALSE(ring_buffer_is_empty(&rb));

    for (uint16_t i = 0; i < 8; i++) {
        uint8_t b;
        ring_buffer_read(&rb, &b, 1);
        ASSERT_EQ(ring_buffer_count(&rb) + ring_buffer_free(&rb), 8);
    }
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
    ASSERT_FALSE(ring_buffer_is_full(&rb));
}

/* ── reset ──────────────────────────────────────────────────────── */

TEST(test_reset)
{
    uint8_t storage[8];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 8);

    uint8_t data[] = {1, 2, 3, 4};
    ring_buffer_write(&rb, data, 4);
    ASSERT_EQ(ring_buffer_count(&rb), 4);

    ring_buffer_reset(&rb);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
    ASSERT_EQ(ring_buffer_count(&rb), 0);
    ASSERT_EQ(ring_buffer_free(&rb), 8);
}

/* ── multiple write-read cycles ─────────────────────────────────── */

TEST(test_multiple_cycles)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);

    for (int cycle = 0; cycle < 10; cycle++) {
        uint8_t w = (uint8_t)(cycle & 0xFF);
        ASSERT_EQ(ring_buffer_write(&rb, &w, 1), 1);
        uint8_t r = 0;
        ASSERT_EQ(ring_buffer_read(&rb, &r, 1), 1);
        ASSERT_EQ(r, w);
    }
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
}

/* ── interleaved producer-consumer ──────────────────────────────── */

TEST(test_interleaved_producer_consumer)
{
    uint8_t storage[8];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 8);

    /* Producer writes 3, consumer reads 2, repeat */
    uint8_t seq = 0;
    uint8_t expected = 0;

    for (int round = 0; round < 5; round++) {
        uint8_t w[3];
        for (int i = 0; i < 3; i++) { w[i] = seq++; }
        ring_buffer_write(&rb, w, 3);

        uint8_t r[2];
        uint16_t n = ring_buffer_read(&rb, r, 2);
        ASSERT_EQ(n, 2);
        ASSERT_EQ(r[0], expected);
        ASSERT_EQ(r[1], (uint8_t)(expected + 1));
        expected = (uint8_t)(expected + 2);
    }
}

/* ── large block spanning wrap point ────────────────────────────── */

TEST(test_large_block_wrap)
{
    uint8_t storage[16];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 16);

    /* Advance head/tail to position 12 */
    uint8_t filler[12];
    memset(filler, 0xAA, sizeof(filler));
    ring_buffer_write(&rb, filler, 12);
    ring_buffer_skip(&rb, 12);
    /* Now head=12, tail=12, count=0 */

    /* Write 10 bytes — wraps from pos 12 through 0 back to pos 6 */
    uint8_t block[10];
    for (int i = 0; i < 10; i++) { block[i] = (uint8_t)(0x30 + i); }
    ASSERT_EQ(ring_buffer_write(&rb, block, 10), 10);

    /* Read them back */
    uint8_t out[10];
    ASSERT_EQ(ring_buffer_read(&rb, out, 10), 10);
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(out[i], (uint8_t)(0x30 + i));
    }
}

/* ── zero-init safety ───────────────────────────────────────────── */

TEST(test_zero_init_safety)
{
    ring_buffer_t rb;
    memset(&rb, 0, sizeof(rb));
    /* Operations on zero-init must not crash */
    ASSERT_EQ(ring_buffer_count(&rb), 0);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
    ASSERT_FALSE(ring_buffer_is_full(&rb));
    ring_buffer_reset(&rb);
}

TEST_MAIN("ring_buffer")
    RUN(test_init_valid);
    RUN(test_init_null_rb);
    RUN(test_init_null_buf);
    RUN(test_init_zero_capacity);
    RUN(test_write_read_single);
    RUN(test_write_fills_exactly);
    RUN(test_write_when_full);
    RUN(test_read_when_empty);
    RUN(test_wraparound);
    RUN(test_partial_write);
    RUN(test_peek_no_consume);
    RUN(test_skip);
    RUN(test_count_free_consistency);
    RUN(test_reset);
    RUN(test_multiple_cycles);
    RUN(test_interleaved_producer_consumer);
    RUN(test_large_block_wrap);
    RUN(test_zero_init_safety);
TEST_MAIN_END()
