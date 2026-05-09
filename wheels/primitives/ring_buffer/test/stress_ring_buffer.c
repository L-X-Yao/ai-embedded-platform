// SPDX-License-Identifier: MIT
/*
 * stress_ring_buffer — 1M write/read cycle data integrity test
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -I.. -I../../../../tooling/harness \
 *       stress_ring_buffer.c ../ring_buffer.c -lm -o /tmp/stress_ring_buffer
 *
 * Scenarios:
 *   1. 1,000,000 single-byte write/read — verify data integrity
 *   2. 1,000,000 multi-byte write/read — verify no corruption
 *   3. Alternating fill-then-drain — verify head/tail/count consistency
 *   4. Write at capacity boundary — verify no overflow
 */

#include "ring_buffer.h"
#include "test_harness.h"
#include <stdint.h>
#include <string.h>

#define ITERATIONS  1000000
#define BUF_SIZE    256

/* ---- tests ---------------------------------------------------------- */

TEST(single_byte_million_cycles)
{
    uint8_t storage[BUF_SIZE];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_SIZE);

    int ok = 1;

    for (int i = 0; i < ITERATIONS; i++)
    {
        uint8_t wr = (uint8_t)(i & 0xFF);
        uint16_t written = ring_buffer_write(&rb, &wr, 1);
        if (written != 1)
        {
            ok = 0;
            break;
        }

        uint8_t rd = 0;
        uint16_t read_n = ring_buffer_read(&rb, &rd, 1);
        if (read_n != 1 || rd != wr)
        {
            ok = 0;
            break;
        }
    }

    ASSERT_TRUE(ok);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
}

TEST(multi_byte_million_cycles)
{
    uint8_t storage[BUF_SIZE];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_SIZE);

    /* Write and read 7-byte chunks (prime size, not power-of-2) */
    const uint16_t chunk = 7;
    int ok = 1;

    for (int i = 0; i < ITERATIONS; i++)
    {
        uint8_t wr_buf[7];
        uint8_t rd_buf[7];

        for (int j = 0; j < (int)chunk; j++)
        {
            wr_buf[j] = (uint8_t)((i + j) & 0xFF);
        }

        uint16_t written = ring_buffer_write(&rb, wr_buf, chunk);
        if (written != chunk)
        {
            ok = 0;
            break;
        }

        uint16_t read_n = ring_buffer_read(&rb, rd_buf, chunk);
        if (read_n != chunk)
        {
            ok = 0;
            break;
        }

        if (memcmp(wr_buf, rd_buf, chunk) != 0)
        {
            ok = 0;
            break;
        }
    }

    ASSERT_TRUE(ok);
    ASSERT_TRUE(ring_buffer_is_empty(&rb));
}

TEST(fill_drain_cycles)
{
    /*
     * Alternate: fill buffer to capacity, then drain completely.
     * Repeat many times — verifies head/tail/count stay consistent
     * across full wrap cycles.
     */
    uint8_t storage[BUF_SIZE];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_SIZE);

    int ok = 1;
    const int cycles = 10000;

    for (int c = 0; c < cycles; c++)
    {
        /* fill */
        for (uint16_t i = 0; i < BUF_SIZE; i++)
        {
            uint8_t b = (uint8_t)((c + i) & 0xFF);
            uint16_t w = ring_buffer_write(&rb, &b, 1);
            if (w != 1)
            {
                ok = 0;
                break;
            }
        }
        if (!ok) break;

        ASSERT_TRUE(ring_buffer_is_full(&rb));

        /* drain and verify */
        for (uint16_t i = 0; i < BUF_SIZE; i++)
        {
            uint8_t b = 0;
            uint16_t r = ring_buffer_read(&rb, &b, 1);
            if (r != 1 || b != (uint8_t)((c + i) & 0xFF))
            {
                ok = 0;
                break;
            }
        }
        if (!ok) break;

        ASSERT_TRUE(ring_buffer_is_empty(&rb));
        ASSERT_EQ(ring_buffer_count(&rb), 0);
        ASSERT_EQ(ring_buffer_free(&rb), BUF_SIZE);
    }

    ASSERT_TRUE(ok);
}

TEST(partial_write_at_boundary)
{
    /*
     * Fill buffer to capacity-1, then try to write 10 bytes.
     * Should only write 1. Repeat many times to verify boundary
     * handling is consistent.
     */
    uint8_t storage[BUF_SIZE];
    ring_buffer_t rb;
    ring_buffer_init(&rb, storage, BUF_SIZE);

    int ok = 1;

    for (int c = 0; c < 100000; c++)
    {
        ring_buffer_reset(&rb);

        /* Fill to capacity-1 */
        uint8_t fill_data[BUF_SIZE];
        memset(fill_data, 0xAA, sizeof(fill_data));
        ring_buffer_write(&rb, fill_data, (uint16_t)(BUF_SIZE - 1));

        /* Try to write 10 — only 1 should succeed */
        uint8_t extra[10] = {0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
                             0x11, 0x22, 0x33, 0x44, 0x55};
        uint16_t w = ring_buffer_write(&rb, extra, 10);
        if (w != 1)
        {
            ok = 0;
            break;
        }
        if (!ring_buffer_is_full(&rb))
        {
            ok = 0;
            break;
        }

        /* Read that last byte and verify it */
        ring_buffer_skip(&rb, (uint16_t)(BUF_SIZE - 1));
        uint8_t check = 0;
        ring_buffer_read(&rb, &check, 1);
        if (check != 0xBB)
        {
            ok = 0;
            break;
        }
    }

    ASSERT_TRUE(ok);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress_ring_buffer")
    RUN(single_byte_million_cycles);
    RUN(multi_byte_million_cycles);
    RUN(fill_drain_cycles);
    RUN(partial_write_at_boundary);
TEST_MAIN_END()
