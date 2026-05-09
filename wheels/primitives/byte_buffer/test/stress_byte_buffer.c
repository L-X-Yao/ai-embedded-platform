// SPDX-License-Identifier: MIT
/*
 * stress_byte_buffer.c — long-running stress tests for byte_buffer
 *
 * Compile (standalone):
 *   gcc -std=c99 -O2 -Wall -Wextra -I.. -I../../../../tooling/harness \
 *       stress_byte_buffer.c ../byte_buffer.c -o /tmp/stress_byte_buffer -lm
 *
 * Scenarios:
 *   1. Fill/consume/compact cycles — 100K iterations, verify no cursor drift
 *   2. Mixed put/get type stress — u8/u16/u32 sequences over 500K ops
 *   3. Overflow rejection — push when full, verify position unchanged
 *   4. Compact-then-refill — verify compact frees space correctly over many cycles
 */

#include "test_harness.h"
#include "byte_buffer.h"
#include <string.h>

#define BUF_SIZE  256

/* ---- tests ---------------------------------------------------------- */

TEST(fill_consume_compact_cycles)
{
    uint8_t storage[BUF_SIZE];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_SIZE);

    int ok = 1;
    for (int i = 0; i < 100000; i++) {
        /* Write 100 bytes */
        for (int j = 0; j < 100; j++) {
            if (byte_buffer_put_u8(&buf, (uint8_t)(j & 0xFF)) != BYTE_BUFFER_OK) {
                ok = 0; break;
            }
        }
        if (!ok) break;
        ASSERT_EQ(100u, byte_buffer_readable(&buf));

        /* Read 50 bytes */
        for (int j = 0; j < 50; j++) {
            uint8_t val;
            if (byte_buffer_get_u8(&buf, &val) != BYTE_BUFFER_OK) { ok = 0; break; }
            if (val != (uint8_t)(j & 0xFF)) { ok = 0; break; }
        }
        if (!ok) break;
        ASSERT_EQ(50u, byte_buffer_readable(&buf));

        /* Compact — moves remaining 50 bytes to front */
        byte_buffer_compact(&buf);
        ASSERT_EQ(50u, byte_buffer_readable(&buf));
        ASSERT_EQ(BUF_SIZE - 50u, byte_buffer_writable(&buf));

        /* Read remaining 50 */
        for (int j = 50; j < 100; j++) {
            uint8_t val;
            if (byte_buffer_get_u8(&buf, &val) != BYTE_BUFFER_OK) { ok = 0; break; }
            if (val != (uint8_t)(j & 0xFF)) { ok = 0; break; }
        }
        if (!ok) break;
        ASSERT_EQ(0u, byte_buffer_readable(&buf));

        /* Reset for next cycle */
        byte_buffer_reset(&buf);
    }
    ASSERT_TRUE(ok);
}

TEST(mixed_type_stress)
{
    uint8_t storage[BUF_SIZE];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_SIZE);

    int ok = 1;
    for (int i = 0; i < 500000; i++) {
        byte_buffer_reset(&buf);

        /* Write: u8 + u16_be + u32_be = 1+2+4 = 7 bytes */
        uint8_t  v8  = (uint8_t)(i & 0xFF);
        uint16_t v16 = (uint16_t)(i & 0xFFFF);
        uint32_t v32 = (uint32_t)i;

        if (byte_buffer_put_u8(&buf, v8) != BYTE_BUFFER_OK) { ok = 0; break; }
        if (byte_buffer_put_u16_be(&buf, v16) != BYTE_BUFFER_OK) { ok = 0; break; }
        if (byte_buffer_put_u32_be(&buf, v32) != BYTE_BUFFER_OK) { ok = 0; break; }
        ASSERT_EQ(7u, byte_buffer_readable(&buf));

        /* Read back and verify */
        uint8_t  r8;
        uint16_t r16;
        uint32_t r32;
        if (byte_buffer_get_u8(&buf, &r8) != BYTE_BUFFER_OK) { ok = 0; break; }
        if (byte_buffer_get_u16_be(&buf, &r16) != BYTE_BUFFER_OK) { ok = 0; break; }
        if (byte_buffer_get_u32_be(&buf, &r32) != BYTE_BUFFER_OK) { ok = 0; break; }
        if (r8 != v8 || r16 != v16 || r32 != v32) { ok = 0; break; }
        ASSERT_EQ(0u, byte_buffer_readable(&buf));
    }
    ASSERT_TRUE(ok);
}

TEST(overflow_no_position_change)
{
    uint8_t storage[BUF_SIZE];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_SIZE);

    int ok = 1;
    for (int cycle = 0; cycle < 100000; cycle++) {
        byte_buffer_reset(&buf);

        /* Fill completely */
        uint8_t fill_block[BUF_SIZE];
        memset(fill_block, 0xAA, sizeof(fill_block));
        byte_buffer_put_bytes(&buf, fill_block, BUF_SIZE);
        ASSERT_EQ(0u, byte_buffer_writable(&buf));

        /* Overflow attempt must fail without changing position */
        size_t written_before = byte_buffer_written(&buf);
        byte_buffer_err_t err = byte_buffer_put_u8(&buf, 0xBB);
        if (err != BYTE_BUFFER_ERR_FULL) { ok = 0; break; }
        if (byte_buffer_written(&buf) != written_before) { ok = 0; break; }
        ASSERT_EQ(BUF_SIZE, byte_buffer_readable(&buf));
    }
    ASSERT_TRUE(ok);
}

TEST(compact_refill_cycles)
{
    uint8_t storage[BUF_SIZE];
    byte_buffer_t buf;
    byte_buffer_init(&buf, storage, BUF_SIZE);

    int ok = 1;
    for (int i = 0; i < 100000; i++) {
        /* Fill half the buffer */
        uint8_t half[BUF_SIZE / 2];
        memset(half, (uint8_t)(i & 0xFF), sizeof(half));
        byte_buffer_put_bytes(&buf, half, BUF_SIZE / 2);

        /* Read a quarter */
        uint8_t tmp[BUF_SIZE / 4];
        byte_buffer_get_bytes(&buf, tmp, BUF_SIZE / 4);

        /* Compact — should recover the read quarter as writable space */
        byte_buffer_compact(&buf);
        size_t readable = byte_buffer_readable(&buf);
        size_t writable = byte_buffer_writable(&buf);
        if (readable != BUF_SIZE / 4) { ok = 0; break; }
        if (writable != BUF_SIZE - BUF_SIZE / 4) { ok = 0; break; }

        /* Drain and reset */
        byte_buffer_reset(&buf);
    }
    ASSERT_TRUE(ok);
}

/* ---- main ----------------------------------------------------------- */

TEST_MAIN("stress byte_buffer")
    RUN(fill_consume_compact_cycles);
    RUN(mixed_type_stress);
    RUN(overflow_no_position_change);
    RUN(compact_refill_cycles);
TEST_MAIN_END()
