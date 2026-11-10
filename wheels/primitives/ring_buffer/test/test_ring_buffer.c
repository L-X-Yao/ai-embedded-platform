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

/* ── RING_BUFFER_ERR_FULL: write to full buffer returns 0 ───────── */

TEST(test_err_full_write_returns_zero)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);

    /* Verify RING_BUFFER_ERR_FULL enum value exists */
    ASSERT_EQ(RING_BUFFER_ERR_FULL, -2);

    /* Fill completely */
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    ASSERT_EQ(ring_buffer_write(&rb, data, 4), 4);
    ASSERT_TRUE(ring_buffer_is_full(&rb));
    ASSERT_EQ(ring_buffer_free(&rb), 0);

    /* Single byte write to full buffer returns 0 */
    uint8_t extra = 0xFF;
    ASSERT_EQ(ring_buffer_write(&rb, &extra, 1), 0);

    /* Multi-byte write to full buffer returns 0 */
    uint8_t more[] = {1, 2, 3};
    ASSERT_EQ(ring_buffer_write(&rb, more, 3), 0);

    /* Buffer contents unchanged */
    ASSERT_TRUE(ring_buffer_is_full(&rb));
    ASSERT_EQ(ring_buffer_count(&rb), 4);
    uint8_t out[4];
    ring_buffer_peek(&rb, out, 4);
    ASSERT_EQ(out[0], 0xAA);
    ASSERT_EQ(out[3], 0xDD);
}

/* ── RING_BUFFER_ERR_EMPTY: read from empty buffer returns 0 ───── */

TEST(test_err_empty_read_returns_zero)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);

    /* Verify RING_BUFFER_ERR_EMPTY enum value exists */
    ASSERT_EQ(RING_BUFFER_ERR_EMPTY, -3);

    /* Buffer starts empty */
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
    ASSERT_EQ(ring_buffer_count(&rb), 0);

    /* Single byte read from empty buffer returns 0 */
    uint8_t out = 0xAA;
    ASSERT_EQ(ring_buffer_read(&rb, &out, 1), 0);
    ASSERT_EQ(out, 0xAA);  /* unchanged */

    /* Multi-byte read from empty buffer returns 0 */
    uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_EQ(ring_buffer_read(&rb, buf, 4), 0);
    ASSERT_EQ(buf[0], 0xFF);  /* unchanged */

    /* Peek from empty buffer also returns 0 */
    ASSERT_EQ(ring_buffer_peek(&rb, buf, 4), 0);

    /* Skip from empty buffer returns 0 */
    ASSERT_EQ(ring_buffer_skip(&rb, 1), 0);

    /* Write then read all, then read again (empty after drain) */
    uint8_t data[] = {1, 2};
    ring_buffer_write(&rb, data, 2);
    ring_buffer_read(&rb, buf, 2);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
    ASSERT_EQ(ring_buffer_read(&rb, buf, 1), 0);
}

/* ── NULL parameter handling (mutation: return 0 → return -1) ──── */

TEST(test_write_null_rb)
{
    uint8_t data = 0;
    ASSERT_EQ(ring_buffer_write(NULL, &data, 1), 0);
}

TEST(test_write_null_data)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);
    ASSERT_EQ(ring_buffer_write(&rb, NULL, 5), 0);
    ASSERT_EQ(ring_buffer_count(&rb), 0);
}

TEST(test_read_null_rb)
{
    uint8_t buf;
    ASSERT_EQ(ring_buffer_read(NULL, &buf, 1), 0);
}

TEST(test_read_null_data)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);
    uint8_t w = 1;
    ring_buffer_write(&rb, &w, 1);
    ASSERT_EQ(ring_buffer_read(&rb, NULL, 1), 0);
    ASSERT_EQ(ring_buffer_count(&rb), 1);
}

TEST(test_peek_null_rb)
{
    uint8_t buf;
    ASSERT_EQ(ring_buffer_peek(NULL, &buf, 1), 0);
}

TEST(test_peek_null_data)
{
    uint8_t storage[4];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 4);
    uint8_t w = 1;
    ring_buffer_write(&rb, &w, 1);
    ASSERT_EQ(ring_buffer_peek(&rb, NULL, 1), 0);
}

TEST(test_skip_null_rb)
{
    ASSERT_EQ(ring_buffer_skip(NULL, 1), 0);
}

TEST(test_count_null_rb)
{
    ASSERT_EQ(ring_buffer_count(NULL), 0);
}

TEST(test_free_null_rb)
{
    ASSERT_EQ(ring_buffer_free(NULL), 0);
}

/* ── peek exact byte count (mutation: i < len → i <= len) ──────── */

TEST(test_peek_exact_bytes)
{
    uint8_t storage[8];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, 8);

    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    ring_buffer_write(&rb, data, 3);

    uint8_t out[8];
    memset(out, 0xFF, sizeof(out));
    uint16_t peeked = ring_buffer_peek(&rb, out, 3);
    ASSERT_EQ(peeked, 3);
    ASSERT_EQ(out[0], 0xAA);
    ASSERT_EQ(out[1], 0xBB);
    ASSERT_EQ(out[2], 0xCC);
    ASSERT_EQ(out[3], 0xFF);  /* sentinel: must NOT be overwritten */
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

/* capacity is 0 here, so skip must not reach its % capacity */
TEST(test_zero_init_skip)
{
    ring_buffer_t rb;
    memset(&rb, 0, sizeof(rb));
    ASSERT_EQ(ring_buffer_skip(&rb, 1), 0);
    ASSERT_EQ(ring_buffer_skip(&rb, 0), 0);
    ASSERT_EQ(ring_buffer_count(&rb), 0);
}

/* -- index laps -------------------------------------------------------- */

/* Fill to capacity and take half out, again and again, until the indexes have
 * gone round more than twice. Returns the number of wrong answers. */
static long lap_errors(uint8_t *storage, uint16_t capacity)
{
    ring_buffer_t rb;
    long errors = 0;
    uint32_t sent = 0;
    uint32_t got = 0;
    uint32_t target = 5U * capacity + 3U;
    uint16_t step = (uint16_t)(capacity / 2U + 1U);

    if (ring_buffer_init(&rb, storage, capacity) != RING_BUFFER_OK) {
        return -1;
    }
    while (got < target) {
        while (!ring_buffer_is_full(&rb)) {
            uint8_t byte = (uint8_t)sent;

            if (ring_buffer_write(&rb, &byte, 1) != 1U) {
                return -1;
            }
            sent++;
        }
        uint8_t extra = 0;

        errors += (ring_buffer_count(&rb) != capacity);
        errors += (ring_buffer_free(&rb) != 0U);
        errors += (ring_buffer_write(&rb, &extra, 1) != 0U);

        for (uint16_t i = 0; i < step; i++) {
            uint8_t byte = 0;

            errors += (ring_buffer_read(&rb, &byte, 1) != 1U);
            errors += (byte != (uint8_t)got);
            got++;
        }
        errors += (ring_buffer_count(&rb) != (uint16_t)(capacity - step));
    }
    return errors;
}

TEST(test_all_of_capacity_is_usable_across_index_laps)
{
    static uint8_t storage[65535];
    static const uint16_t sizes[] = {1, 2, 3, 8, 251, 32768, 65535};

    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        ASSERT_EQ(0, lap_errors(storage, sizes[i]));
    }
}

TEST(test_reset_away_from_zero_empties_and_keeps_capacity)
{
    uint8_t storage[5];
    ring_buffer_t rb;
    const uint8_t first[3] = {1, 2, 3};
    const uint8_t second[5] = {5, 6, 7, 8, 9};
    uint8_t out[5];

    ring_buffer_init(&rb, storage, 5);
    ASSERT_EQ(3, ring_buffer_write(&rb, first, 3));
    ASSERT_EQ(2, ring_buffer_read(&rb, out, 2));

    ring_buffer_reset(&rb);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
    ASSERT_EQ(0, ring_buffer_count(&rb));
    ASSERT_EQ(5, ring_buffer_free(&rb));
    ASSERT_EQ(0, ring_buffer_read(&rb, out, 5));

    ASSERT_EQ(5, ring_buffer_write(&rb, second, 5));
    ASSERT_TRUE(ring_buffer_is_full(&rb));
    ASSERT_EQ(5, ring_buffer_read(&rb, out, 5));
    ASSERT_MEMEQ_N(second, out, 5);
}

TEST_MAIN("ring_buffer")
    RUN(test_zero_init_skip);
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
    RUN(test_err_full_write_returns_zero);
    RUN(test_err_empty_read_returns_zero);
    RUN(test_write_null_rb);
    RUN(test_write_null_data);
    RUN(test_read_null_rb);
    RUN(test_read_null_data);
    RUN(test_peek_null_rb);
    RUN(test_peek_null_data);
    RUN(test_skip_null_rb);
    RUN(test_count_null_rb);
    RUN(test_free_null_rb);
    RUN(test_peek_exact_bytes);
    RUN(test_zero_init_safety);
    RUN(test_all_of_capacity_is_usable_across_index_laps);
    RUN(test_reset_away_from_zero_empties_and_keeps_capacity);
TEST_MAIN_END()
