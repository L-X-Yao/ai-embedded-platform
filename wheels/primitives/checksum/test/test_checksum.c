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
        sum += (unsigned)(((uint16_t)data[i] << 8) | data[i + 1]);
    sum += cs;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    ASSERT_EQ((uint16_t)sum, 0xFFFF);
}

/* An odd last byte is the high byte of a word padded with zero (RFC 1071).
 * 4500 + 0000 gives ~4500 = BAFF.  That tail is zero, so dropping the byte
 * would not show: 4500 + AB00 gives ~F000 = 0FFF. */
TEST(test_internet_odd_length)
{
    uint8_t data[] = {0x45, 0x00, 0x00};
    ASSERT_EQ_HEX(checksum_internet(data, 3), 0xBAFF);
    uint8_t tail[] = {0x45, 0x00, 0xAB};
    ASSERT_EQ_HEX(checksum_internet(tail, 3), 0x0FFF);
}

TEST(test_internet_null)
{
    ASSERT_EQ(checksum_internet(NULL, 0), 0);
}

/* RFC 1071 section 3: these words sum to DDF2, so the checksum is ~DDF2. */
TEST(test_internet_rfc1071_known_answer)
{
    uint8_t data[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    ASSERT_EQ_HEX(checksum_internet(data, 8), 0x220D);
}

/* FFFF + FFFF + 0001 = 1FFFF.  The first fold gives 10000 and the second is
 * needed to get to 0001, so the checksum is ~0001 = FFFE. */
TEST(test_internet_both_folds_needed)
{
    uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01};
    ASSERT_EQ_HEX(checksum_internet(data, 6), 0xFFFE);
}

/* Known answers: sum1 is the byte sum mod 255, sum2 the sum of the running
 * sum1 mod 255, the result (sum2 << 8) | sum1.  By hand for "abcde":
 * sum1 97 195 39 139 240 = F0, sum2 97 37 76 215 200 = C8. */
TEST(test_fletcher16_known_answers)
{
    ASSERT_EQ_HEX(checksum_fletcher16((const uint8_t *)"abcde", 5), 0xC8F0);
    ASSERT_EQ_HEX(checksum_fletcher16((const uint8_t *)"abcdef", 6), 0x2057);
    ASSERT_EQ_HEX(checksum_fletcher16((const uint8_t *)"abcdefgh", 8), 0x0627);
}

/* 0xFF is 0 mod 255 and no data leaves both sums at 0: the result must be
 * reduced to 0, not left at the 0xFF the sums start from. */
TEST(test_fletcher16_zero_residue)
{
    uint8_t ff[1000];
    memset(ff, 0xFF, sizeof(ff));
    ASSERT_EQ_HEX(checksum_fletcher16(ff, 0), 0x0000);
    ASSERT_EQ_HEX(checksum_fletcher16(ff, 1), 0x0000);
    ASSERT_EQ_HEX(checksum_fletcher16(ff, sizeof(ff)), 0x0000);
}

/* The same definition with no blocks: Fletcher-16 of data[0..len). */
static uint16_t fletcher16_by_definition(const uint8_t *data, size_t len)
{
    unsigned sum1 = 0, sum2 = 0;
    for (size_t i = 0; i < len; i++) {
        sum1 = (sum1 + data[i]) % 255u;
        sum2 = (sum2 + sum1) % 255u;
    }
    return (uint16_t)((sum2 << 8) | sum1);
}

/* Every length across the 20-byte blocks, with mixed and with large bytes */
TEST(test_fletcher16_matches_definition)
{
    uint8_t mixed[300], large[300];
    for (size_t i = 0; i < sizeof(mixed); i++) {
        mixed[i] = (uint8_t)(i * 37u + 11u);
        large[i] = 0xFE;
    }

    for (size_t len = 0; len <= sizeof(mixed); len++) {
        ASSERT_EQ_HEX(checksum_fletcher16(mixed, len),
                      fletcher16_by_definition(mixed, len));
        ASSERT_EQ_HEX(checksum_fletcher16(large, len),
                      fletcher16_by_definition(large, len));
    }
}

TEST_MAIN("checksum")
    RUN(test_internet_rfc1071_known_answer);
    RUN(test_internet_both_folds_needed);
    RUN(test_fletcher16_known_answers);
    RUN(test_fletcher16_zero_residue);
    RUN(test_fletcher16_matches_definition);
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
