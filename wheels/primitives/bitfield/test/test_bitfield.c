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
TEST_MAIN_END()
