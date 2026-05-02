// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "checksum.h"
#include <string.h>

TEST(test_xor_basic)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    ASSERT_EQ(checksum_xor(data, 3), 0x00);
    uint8_t d2[] = {0xFF, 0x00, 0xAA};
    ASSERT_EQ(checksum_xor(d2, 3), 0x55);
}

TEST(test_xor_null)
{
    ASSERT_EQ(checksum_xor(NULL, 0), 0);
}

TEST(test_sum8)
{
    uint8_t data[] = {1, 2, 3, 4, 5};
    ASSERT_EQ(checksum_sum8(data, 5), 15);
    uint8_t d2[] = {0xFF, 0x01};
    ASSERT_EQ(checksum_sum8(d2, 2), 0x00);
}

TEST(test_fletcher16)
{
    uint8_t data[] = {0x01, 0x02};
    uint16_t f = checksum_fletcher16(data, 2);
    ASSERT_EQ(f != 0, 1);
}

TEST(test_fletcher16_consistent)
{
    uint8_t data[] = "abcde";
    uint16_t f1 = checksum_fletcher16(data, 5);
    uint16_t f2 = checksum_fletcher16(data, 5);
    ASSERT_EQ(f1, f2);
}

TEST(test_lrc)
{
    uint8_t data[] = {0x02, 0x30, 0x31};
    uint8_t lrc = checksum_lrc(data, 3);
    uint8_t verify = (uint8_t)(data[0] + data[1] + data[2] + lrc);
    ASSERT_EQ(verify, 0);
}

TEST(test_internet_rfc1071)
{
    uint8_t data[] = {0x00, 0x01, 0xf2, 0x03,
                      0xf4, 0xf5, 0xf6, 0xf7};
    uint16_t cs = checksum_internet(data, 8);
    uint32_t sum = 0;
    for (int i = 0; i < 8; i += 2)
        sum += ((uint16_t)data[i] << 8) | data[i + 1];
    sum += cs;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    ASSERT_EQ((uint16_t)sum, 0xFFFF);
}

TEST(test_internet_odd_length)
{
    uint8_t data[] = {0x45, 0x00, 0x00};
    uint16_t cs = checksum_internet(data, 3);
    ASSERT_EQ(cs != 0, 1);
}

TEST(test_internet_null)
{
    ASSERT_EQ(checksum_internet(NULL, 0), 0);
}

TEST_MAIN("checksum")
    RUN(test_xor_basic);
    RUN(test_xor_null);
    RUN(test_sum8);
    RUN(test_fletcher16);
    RUN(test_fletcher16_consistent);
    RUN(test_lrc);
    RUN(test_internet_rfc1071);
    RUN(test_internet_odd_length);
    RUN(test_internet_null);
TEST_MAIN_END()
