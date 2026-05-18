// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "bcd.h"

/* ---------- bcd_is_valid ---------- */

TEST(test_valid_bcd_bytes)
{
    ASSERT_TRUE(bcd_is_valid(0x00));
    ASSERT_TRUE(bcd_is_valid(0x09));
    ASSERT_TRUE(bcd_is_valid(0x10));
    ASSERT_TRUE(bcd_is_valid(0x59));
    ASSERT_TRUE(bcd_is_valid(0x99));
}

TEST(test_invalid_bcd_bytes)
{
    ASSERT_FALSE(bcd_is_valid(0x0A));
    ASSERT_FALSE(bcd_is_valid(0xA0));
    ASSERT_FALSE(bcd_is_valid(0xFF));
    ASSERT_FALSE(bcd_is_valid(0x1B));
    ASSERT_FALSE(bcd_is_valid(0xC3));
}

/* ---------- bcd_to_bin ---------- */

TEST(test_to_bin_zero)
{
    ASSERT_EQ(bcd_to_bin(0x00), 0);
}

TEST(test_to_bin_single_digit)
{
    ASSERT_EQ(bcd_to_bin(0x07), 7);
}

TEST(test_to_bin_two_digits)
{
    ASSERT_EQ(bcd_to_bin(0x59), 59);
    ASSERT_EQ(bcd_to_bin(0x23), 23);
    ASSERT_EQ(bcd_to_bin(0x99), 99);
}

TEST(test_to_bin_rtc_time)
{
    ASSERT_EQ(bcd_to_bin(0x12), 12);
    ASSERT_EQ(bcd_to_bin(0x30), 30);
    ASSERT_EQ(bcd_to_bin(0x45), 45);
}

/* ---------- bcd_from_bin ---------- */

TEST(test_from_bin_zero)
{
    ASSERT_EQ(bcd_from_bin(0), 0x00);
}

TEST(test_from_bin_single_digit)
{
    ASSERT_EQ(bcd_from_bin(7), 0x07);
}

TEST(test_from_bin_two_digits)
{
    ASSERT_EQ(bcd_from_bin(59), 0x59);
    ASSERT_EQ(bcd_from_bin(23), 0x23);
    ASSERT_EQ(bcd_from_bin(99), 0x99);
}

/* ---------- roundtrip ---------- */

TEST(test_roundtrip_all_valid)
{
    for (uint8_t bin = 0; bin <= 99; bin++) {
        uint8_t bcd = bcd_from_bin(bin);
        ASSERT_TRUE(bcd_is_valid(bcd));
        ASSERT_EQ(bcd_to_bin(bcd), bin);
    }
}

/* ---------- safe variants ---------- */

TEST(test_to_bin_safe_ok)
{
    uint8_t out = 0;
    ASSERT_EQ(bcd_to_bin_safe(0x59, &out), BCD_OK);
    ASSERT_EQ(out, 59);
}

TEST(test_to_bin_safe_null)
{
    ASSERT_EQ(bcd_to_bin_safe(0x59, NULL), BCD_ERR_PARAM);
}

TEST(test_to_bin_safe_invalid)
{
    uint8_t out = 0;
    ASSERT_EQ(bcd_to_bin_safe(0xAB, &out), BCD_ERR_INVALID);
}

TEST(test_from_bin_safe_ok)
{
    uint8_t out = 0;
    ASSERT_EQ(bcd_from_bin_safe(59, &out), BCD_OK);
    ASSERT_EQ(out, 0x59);
}

TEST(test_from_bin_safe_null)
{
    ASSERT_EQ(bcd_from_bin_safe(59, NULL), BCD_ERR_PARAM);
}

TEST(test_from_bin_safe_range)
{
    uint8_t out = 0;
    ASSERT_EQ(bcd_from_bin_safe(100, &out), BCD_ERR_RANGE);
    ASSERT_EQ(bcd_from_bin_safe(255, &out), BCD_ERR_RANGE);
}

/* ---------- buffer conversions ---------- */

TEST(test_buf_to_bin_rtc_datetime)
{
    uint8_t bcd[] = {0x26, 0x05, 0x13, 0x14, 0x30, 0x00};
    uint8_t bin[6];
    ASSERT_EQ(bcd_buf_to_bin(bcd, bin, 6), BCD_OK);
    ASSERT_EQ(bin[0], 26);
    ASSERT_EQ(bin[1], 5);
    ASSERT_EQ(bin[2], 13);
    ASSERT_EQ(bin[3], 14);
    ASSERT_EQ(bin[4], 30);
    ASSERT_EQ(bin[5], 0);
}

TEST(test_buf_to_bin_null)
{
    uint8_t buf[4];
    ASSERT_EQ(bcd_buf_to_bin(NULL, buf, 4), BCD_ERR_PARAM);
    ASSERT_EQ(bcd_buf_to_bin(buf, NULL, 4), BCD_ERR_PARAM);
}

TEST(test_buf_to_bin_invalid_stops)
{
    uint8_t bcd[] = {0x12, 0xFF, 0x34};
    uint8_t bin[3] = {0};
    ASSERT_EQ(bcd_buf_to_bin(bcd, bin, 3), BCD_ERR_INVALID);
    ASSERT_EQ(bin[0], 12);
}

TEST(test_buf_from_bin_ok)
{
    uint8_t bin[] = {26, 5, 13};
    uint8_t bcd[3];
    ASSERT_EQ(bcd_buf_from_bin(bin, bcd, 3), BCD_OK);
    ASSERT_EQ(bcd[0], 0x26);
    ASSERT_EQ(bcd[1], 0x05);
    ASSERT_EQ(bcd[2], 0x13);
}

TEST(test_buf_from_bin_null)
{
    uint8_t buf[4];
    ASSERT_EQ(bcd_buf_from_bin(NULL, buf, 4), BCD_ERR_PARAM);
    ASSERT_EQ(bcd_buf_from_bin(buf, NULL, 4), BCD_ERR_PARAM);
}

TEST(test_buf_from_bin_range_stops)
{
    uint8_t bin[] = {12, 100, 34};
    uint8_t bcd[3] = {0};
    ASSERT_EQ(bcd_buf_from_bin(bin, bcd, 3), BCD_ERR_RANGE);
    ASSERT_EQ(bcd[0], 0x12);
}

TEST(test_buf_zero_length)
{
    uint8_t a[1] = {0x99}, b[1] = {0};
    ASSERT_EQ(bcd_buf_to_bin(a, b, 0), BCD_OK);
    ASSERT_EQ(bcd_buf_from_bin(b, a, 0), BCD_OK);
}

TEST_MAIN("bcd")
    RUN(test_valid_bcd_bytes);
    RUN(test_invalid_bcd_bytes);
    RUN(test_to_bin_zero);
    RUN(test_to_bin_single_digit);
    RUN(test_to_bin_two_digits);
    RUN(test_to_bin_rtc_time);
    RUN(test_from_bin_zero);
    RUN(test_from_bin_single_digit);
    RUN(test_from_bin_two_digits);
    RUN(test_roundtrip_all_valid);
    RUN(test_to_bin_safe_ok);
    RUN(test_to_bin_safe_null);
    RUN(test_to_bin_safe_invalid);
    RUN(test_from_bin_safe_ok);
    RUN(test_from_bin_safe_null);
    RUN(test_from_bin_safe_range);
    RUN(test_buf_to_bin_rtc_datetime);
    RUN(test_buf_to_bin_null);
    RUN(test_buf_to_bin_invalid_stops);
    RUN(test_buf_from_bin_ok);
    RUN(test_buf_from_bin_null);
    RUN(test_buf_from_bin_range_stops);
    RUN(test_buf_zero_length);
TEST_MAIN_END()
