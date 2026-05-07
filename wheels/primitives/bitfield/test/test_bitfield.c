// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "bitfield.h"
#include <string.h>

TEST(test_get_aligned_byte)
{
    uint8_t data[] = {0xAB, 0xCD};
    uint32_t val;
    ASSERT_EQ(bitfield_get(data, 2, 0, 8, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0xAB);
    ASSERT_EQ(bitfield_get(data, 2, 8, 8, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0xCD);
}

TEST(test_get_unaligned)
{
    uint8_t data[] = {0xD6, 0xB0};  /* 11010110 10110000 */
    uint32_t val;
    ASSERT_EQ(bitfield_get(data, 2, 3, 5, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0x16);  /* 10110 */
    ASSERT_EQ(bitfield_get(data, 2, 4, 8, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0x6B);  /* 01101011 */
}

TEST(test_get_16bit)
{
    uint8_t data[] = {0x12, 0x34, 0x56};
    uint32_t val;
    ASSERT_EQ(bitfield_get(data, 3, 0, 16, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0x1234);
    ASSERT_EQ(bitfield_get(data, 3, 8, 16, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0x3456);
}

TEST(test_get_32bit)
{
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t val;
    ASSERT_EQ(bitfield_get(data, 4, 0, 32, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0xDEADBEEFu);
}

TEST(test_set_aligned)
{
    uint8_t data[2] = {0};
    ASSERT_EQ(bitfield_set(data, 2, 0, 8, 0xAB), BITFIELD_OK);
    ASSERT_EQ(data[0], 0xAB);
    ASSERT_EQ(bitfield_set(data, 2, 8, 8, 0xCD), BITFIELD_OK);
    ASSERT_EQ(data[1], 0xCD);
}

TEST(test_set_unaligned)
{
    uint8_t data[2] = {0};
    ASSERT_EQ(bitfield_set(data, 2, 2, 4, 0x0D), BITFIELD_OK);
    ASSERT_EQ(data[0], 0x34);  /* 00110100 */
}

TEST(test_roundtrip)
{
    uint8_t data[4] = {0};
    ASSERT_EQ(bitfield_set(data, 4, 3, 17, 0x1ABCD), BITFIELD_OK);
    uint32_t val;
    ASSERT_EQ(bitfield_get(data, 4, 3, 17, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0x1ABCD);
}

TEST(test_single_bit_ops)
{
    uint8_t data[2] = {0};
    bitfield_set_bit(data, 2, 0);
    ASSERT_TRUE(bitfield_test_bit(data, 2, 0));
    ASSERT_EQ(data[0], 0x80);

    bitfield_set_bit(data, 2, 15);
    ASSERT_TRUE(bitfield_test_bit(data, 2, 15));
    ASSERT_EQ(data[1], 0x01);

    bitfield_clear_bit(data, 2, 0);
    ASSERT_FALSE(bitfield_test_bit(data, 2, 0));
    ASSERT_EQ(data[0], 0x00);
}

TEST(test_null_safety)
{
    uint32_t val;
    ASSERT_EQ(bitfield_get(NULL, 0, 0, 8, &val), BITFIELD_ERR_NULL);
    ASSERT_EQ(bitfield_set(NULL, 0, 0, 8, 0), BITFIELD_ERR_NULL);
    uint8_t d[1] = {0};
    ASSERT_EQ(bitfield_get(d, 1, 0, 8, NULL), BITFIELD_ERR_NULL);
}

TEST(test_range_check)
{
    uint8_t data[1] = {0xFF};
    uint32_t val;
    ASSERT_EQ(bitfield_get(data, 1, 0, 9, &val), BITFIELD_ERR_RANGE);
    ASSERT_EQ(bitfield_get(data, 1, 4, 8, &val), BITFIELD_ERR_RANGE);
}

TEST(test_width_check)
{
    uint8_t data[5] = {0};
    uint32_t val;
    ASSERT_EQ(bitfield_get(data, 5, 0, 0, &val), BITFIELD_ERR_WIDTH);
    ASSERT_EQ(bitfield_get(data, 5, 0, 33, &val), BITFIELD_ERR_WIDTH);
}

TEST(test_oob_bit_ops)
{
    uint8_t data[1] = {0xFF};
    ASSERT_FALSE(bitfield_test_bit(data, 1, 8));
    bitfield_set_bit(data, 1, 8);
    ASSERT_EQ(data[0], 0xFF);
}

/* ── Mutation-hardening tests ─────────────────────────────────── */

/*
 * Bit ordering: bit 0 is MSB of byte 0 (big-endian bit numbering).
 * Verify each individual bit position maps to the correct byte/mask.
 * Catches 7 - (pos % 8) vs pos % 8 mutations and / vs % swaps.
 */
TEST(test_bit_ordering_msb_first)
{
    uint8_t data[1] = {0x80}; /* 10000000 */
    uint32_t val;

    /* bit 0 = MSB of byte 0 = 1 */
    ASSERT_EQ(bitfield_get(data, 1, 0, 1, &val), BITFIELD_OK);
    ASSERT_EQ(val, 1u);

    /* bit 7 = LSB of byte 0 = 0 */
    ASSERT_EQ(bitfield_get(data, 1, 7, 1, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0u);

    data[0] = 0x01; /* 00000001 */
    /* bit 7 = LSB of byte 0 = 1 */
    ASSERT_EQ(bitfield_get(data, 1, 7, 1, &val), BITFIELD_OK);
    ASSERT_EQ(val, 1u);

    /* bit 0 = MSB of byte 0 = 0 */
    ASSERT_EQ(bitfield_get(data, 1, 0, 1, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0u);
}

/*
 * bitfield_get: extract each single bit of a known pattern and verify.
 * Catches byte_idx = pos / 8 vs pos % 8, or bit_idx inversion errors.
 */
TEST(test_get_each_bit_individually)
{
    /* 0b10110010 = 0xB2 */
    uint8_t data[1] = {0xB2};
    uint32_t val;
    /* Pattern: bits 0..7 = 1,0,1,1,0,0,1,0 */
    uint32_t expected[8] = {1, 0, 1, 1, 0, 0, 1, 0};
    for (size_t i = 0; i < 8; i++) {
        ASSERT_EQ(bitfield_get(data, 1, i, 1, &val), BITFIELD_OK);
        ASSERT_EQ(val, expected[i]);
    }
}

/*
 * bitfield_set: set each individual bit and verify exactly one bit changes.
 * Catches |= vs &= confusion and bit_idx computation errors.
 */
TEST(test_set_each_bit_individually)
{
    for (size_t i = 0; i < 8; i++) {
        uint8_t data[1] = {0x00};
        ASSERT_EQ(bitfield_set(data, 1, i, 1, 1), BITFIELD_OK);
        /* Only bit i should be set */
        uint32_t val;
        ASSERT_EQ(bitfield_get(data, 1, i, 1, &val), BITFIELD_OK);
        ASSERT_EQ(val, 1u);
        /* All other bits must remain 0 */
        for (size_t j = 0; j < 8; j++) {
            if (j == i) continue;
            ASSERT_EQ(bitfield_get(data, 1, j, 1, &val), BITFIELD_OK);
            ASSERT_EQ(val, 0u);
        }
    }
}

/*
 * bitfield_set: clear each individual bit from an all-ones byte —
 * verifies the &= ~mask path (separate from the |= path).
 */
TEST(test_clear_each_bit_individually)
{
    for (size_t i = 0; i < 8; i++) {
        uint8_t data[1] = {0xFF};
        ASSERT_EQ(bitfield_set(data, 1, i, 1, 0), BITFIELD_OK);
        uint32_t val;
        ASSERT_EQ(bitfield_get(data, 1, i, 1, &val), BITFIELD_OK);
        ASSERT_EQ(val, 0u);
        /* All other bits must remain 1 */
        for (size_t j = 0; j < 8; j++) {
            if (j == i) continue;
            ASSERT_EQ(bitfield_get(data, 1, j, 1, &val), BITFIELD_OK);
            ASSERT_EQ(val, 1u);
        }
    }
}

/*
 * bitfield_test_bit: verify true/false exactly at boundary bits —
 * catches bit_idx = 7 - (pos % 8) vs pos % 8, and != 0 vs == 0.
 */
TEST(test_test_bit_boundary)
{
    uint8_t data[2] = {0x80, 0x01}; /* 10000000 00000001 */

    /* bit 0: MSB of byte 0 → set */
    ASSERT_TRUE(bitfield_test_bit(data, 2, 0));
    /* bit 7: LSB of byte 0 → clear */
    ASSERT_FALSE(bitfield_test_bit(data, 2, 7));
    /* bit 8: MSB of byte 1 → clear */
    ASSERT_FALSE(bitfield_test_bit(data, 2, 8));
    /* bit 15: LSB of byte 1 → set */
    ASSERT_TRUE(bitfield_test_bit(data, 2, 15));
}

/*
 * bitfield_set_bit / bitfield_clear_bit: operate on specific byte boundary bits —
 * catches byte_idx = bit_index / 8 vs bit_index % 8 mutations.
 */
TEST(test_set_clear_bit_cross_byte_boundary)
{
    uint8_t data[2] = {0x00, 0x00};

    /* Set bit 8 = MSB of byte 1 */
    bitfield_set_bit(data, 2, 8);
    ASSERT_EQ(data[0], 0x00); /* byte 0 untouched */
    ASSERT_EQ(data[1], 0x80); /* byte 1 MSB set */

    /* Clear bit 8 */
    bitfield_clear_bit(data, 2, 8);
    ASSERT_EQ(data[1], 0x00);

    /* Set bit 15 = LSB of byte 1 */
    bitfield_set_bit(data, 2, 15);
    ASSERT_EQ(data[0], 0x00);
    ASSERT_EQ(data[1], 0x01);

    /* Clear bit 15 */
    bitfield_clear_bit(data, 2, 15);
    ASSERT_EQ(data[1], 0x00);
}

/*
 * bitfield_get width=1 and width=32: verify the extreme valid widths work —
 * catches bit_count > 32 vs >= 32, and bit_count == 0 vs < 1 mutations.
 */
TEST(test_width_boundaries_valid)
{
    uint8_t data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t val;

    /* width=1: minimum valid */
    ASSERT_EQ(bitfield_get(data, 4, 0, 1, &val), BITFIELD_OK);
    ASSERT_EQ(val, 1u);

    /* width=32: maximum valid */
    ASSERT_EQ(bitfield_get(data, 4, 0, 32, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0xFFFFFFFFu);
}

/*
 * bitfield_get/set: range boundary — bit_offset + bit_count == data_len*8
 * is valid (exactly fills the array); one more is invalid —
 * catches > vs >= in the range guard.
 */
TEST(test_range_exact_boundary)
{
    uint8_t data[2] = {0xAB, 0xCD};
    uint32_t val;

    /* bit_offset=8, bit_count=8: last 8 bits — exactly valid */
    ASSERT_EQ(bitfield_get(data, 2, 8, 8, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0xCDu);

    /* bit_offset=8, bit_count=9: 8+9=17 > 16 — must error */
    ASSERT_EQ(bitfield_get(data, 2, 8, 9, &val), BITFIELD_ERR_RANGE);

    /* bit_offset=0, bit_count=16: full 16-bit span — valid */
    ASSERT_EQ(bitfield_get(data, 2, 0, 16, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0xABCDu);
}

/*
 * bitfield_get MSB-first value reconstruction: for a multi-bit field,
 * verify that bit 0 of the extracted value corresponds to the first
 * bit in the stream (MSB-first), not the last —
 * catches bit_count-1-i vs i in the result bit placement.
 */
TEST(test_get_msb_first_value_reconstruction)
{
    /* 3-bit field at offset 0: bits are [MSB..LSB] = [1,0,1] → value = 5 */
    uint8_t data[1] = {0xA0}; /* 10100000 */
    uint32_t val;
    ASSERT_EQ(bitfield_get(data, 1, 0, 3, &val), BITFIELD_OK);
    ASSERT_EQ(val, 5u); /* 101 binary = 5 */

    /* 4-bit field at offset 0: 1010 = 10 */
    ASSERT_EQ(bitfield_get(data, 1, 0, 4, &val), BITFIELD_OK);
    ASSERT_EQ(val, 10u);

    /* 4-bit field at offset 4: 0000 = 0 */
    ASSERT_EQ(bitfield_get(data, 1, 4, 4, &val), BITFIELD_OK);
    ASSERT_EQ(val, 0u);
}

/*
 * bitfield_set value bit placement: set a known value and verify the exact
 * byte layout — catches i vs bit_count-1-i mutations.
 */
TEST(test_set_msb_first_value_placement)
{
    uint8_t data[1] = {0x00};

    /* Set 3-bit field at offset 0 to value 5 (101 binary) */
    ASSERT_EQ(bitfield_set(data, 1, 0, 3, 5u), BITFIELD_OK);
    /* Bits 0,1,2 = 1,0,1 → byte = 1010 0000 = 0xA0 */
    ASSERT_EQ(data[0], 0xA0);

    data[0] = 0x00;
    /* Set 4-bit field at offset 4 to value 0xC (1100 binary) */
    ASSERT_EQ(bitfield_set(data, 1, 4, 4, 0xCu), BITFIELD_OK);
    /* Bottom nibble: 1100 = 0x0C */
    ASSERT_EQ(data[0], 0x0C);
}

/*
 * bitfield_set preserves surrounding bits: only the target bits change —
 * catches missing mask in &= ~bit path.
 */
TEST(test_set_preserves_surrounding_bits)
{
    uint8_t data[1] = {0xFF};
    /* Clear bits 2..5 (4 bits) to 0 — surrounding bits 0,1,6,7 must stay 1 */
    ASSERT_EQ(bitfield_set(data, 1, 2, 4, 0u), BITFIELD_OK);
    /* bits 0,1 = 11; bits 2..5 = 0000; bits 6,7 = 11 → 0b11000011 = 0xC3 */
    ASSERT_EQ(data[0], 0xC3);

    data[0] = 0x00;
    /* Set bits 2..5 to 0xF (all 1s) — surrounding bits must stay 0 */
    ASSERT_EQ(bitfield_set(data, 1, 2, 4, 0xFu), BITFIELD_OK);
    /* 0b00111100 = 0x3C */
    ASSERT_EQ(data[0], 0x3C);
}

/*
 * bitfield_get: data_len=0 with valid pointers — should return BITFIELD_ERR_RANGE
 * because bit_offset + bit_count > 0 (any positive bit_count overflows).
 */
TEST(test_zero_length_buffer)
{
    uint8_t dummy[1] = {0xFF};
    uint32_t val;
    /* data_len=0 means 0 bits available; any bit_count >= 1 is out of range */
    int rc = bitfield_get(dummy, 0, 0, 1, &val);
    ASSERT_TRUE(rc != BITFIELD_OK); /* ERR_RANGE expected */
}

/*
 * bitfield_test_bit OOB returns false (not true) —
 * catches negated condition in the bounds guard.
 */
TEST(test_test_bit_oob_returns_false)
{
    uint8_t data[1] = {0xFF}; /* all bits set */
    /* bit 8 is out of bounds for a 1-byte buffer — must return false */
    ASSERT_FALSE(bitfield_test_bit(data, 1, 8));
    ASSERT_FALSE(bitfield_test_bit(data, 1, 100));
}

TEST_MAIN("bitfield")
    RUN(test_get_aligned_byte);
    RUN(test_get_unaligned);
    RUN(test_get_16bit);
    RUN(test_get_32bit);
    RUN(test_set_aligned);
    RUN(test_set_unaligned);
    RUN(test_roundtrip);
    RUN(test_single_bit_ops);
    RUN(test_null_safety);
    RUN(test_range_check);
    RUN(test_width_check);
    RUN(test_oob_bit_ops);
    RUN(test_bit_ordering_msb_first);
    RUN(test_get_each_bit_individually);
    RUN(test_set_each_bit_individually);
    RUN(test_clear_each_bit_individually);
    RUN(test_test_bit_boundary);
    RUN(test_set_clear_bit_cross_byte_boundary);
    RUN(test_width_boundaries_valid);
    RUN(test_range_exact_boundary);
    RUN(test_get_msb_first_value_reconstruction);
    RUN(test_set_msb_first_value_placement);
    RUN(test_set_preserves_surrounding_bits);
    RUN(test_zero_length_buffer);
    RUN(test_test_bit_oob_returns_false);
TEST_MAIN_END()
