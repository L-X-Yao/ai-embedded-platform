// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "bitstream.h"
#include <string.h>

/* ---------- init ---------- */

TEST(test_init_ok)
{
    uint8_t buf[4] = {0};
    bitstream_t bs;
    ASSERT_EQ(bitstream_init(&bs, buf, 4), BITSTREAM_OK);
    ASSERT_EQ(bitstream_tell(&bs), 0u);
}

TEST(test_init_null)
{
    uint8_t buf[4];
    ASSERT_EQ(bitstream_init(NULL, buf, 4), BITSTREAM_ERR_PARAM);
    bitstream_t bs;
    ASSERT_EQ(bitstream_init(&bs, NULL, 4), BITSTREAM_ERR_PARAM);
}

/* ---------- write + read roundtrip ---------- */

TEST(test_write_read_8bit)
{
    uint8_t buf[4] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 4);
    ASSERT_EQ(bitstream_write(&bs, 0xAB, 8), BITSTREAM_OK);
    bitstream_seek(&bs, 0);
    uint32_t val = 0;
    ASSERT_EQ(bitstream_read(&bs, 8, &val), BITSTREAM_OK);
    ASSERT_EQ(val, 0xABu);
}

TEST(test_write_read_11bit)
{
    uint8_t buf[4] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 4);
    ASSERT_EQ(bitstream_write(&bs, 992, 11), BITSTREAM_OK);
    bitstream_seek(&bs, 0);
    uint32_t val = 0;
    bitstream_read(&bs, 11, &val);
    ASSERT_EQ(val, 992u);
}

TEST(test_write_read_1bit)
{
    uint8_t buf[1] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 1);
    bitstream_write(&bs, 1, 1);
    bitstream_seek(&bs, 0);
    uint32_t val = 0;
    bitstream_read(&bs, 1, &val);
    ASSERT_EQ(val, 1u);
}

TEST(test_write_read_32bit)
{
    uint8_t buf[4] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 4);
    ASSERT_EQ(bitstream_write(&bs, 0xDEADBEEF, 32), BITSTREAM_OK);
    bitstream_seek(&bs, 0);
    uint32_t val = 0;
    bitstream_read(&bs, 32, &val);
    ASSERT_EQ(val, 0xDEADBEEFu);
}

TEST(test_sequential_writes)
{
    uint8_t buf[4] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 4);
    bitstream_write(&bs, 5, 3);
    bitstream_write(&bs, 7, 3);
    bitstream_write(&bs, 1, 2);
    ASSERT_EQ(bitstream_tell(&bs), 8u);
    bitstream_seek(&bs, 0);
    uint32_t a = 0, b = 0, c = 0;
    bitstream_read(&bs, 3, &a);
    bitstream_read(&bs, 3, &b);
    bitstream_read(&bs, 2, &c);
    ASSERT_EQ(a, 5u);
    ASSERT_EQ(b, 7u);
    ASSERT_EQ(c, 1u);
}

TEST(test_16_channels_11bit)
{
    uint8_t buf[22] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 22);
    for (int i = 0; i < 16; i++)
        bitstream_write(&bs, (uint32_t)(172 + i * 100), 11);
    bitstream_seek(&bs, 0);
    for (int i = 0; i < 16; i++) {
        uint32_t val = 0;
        bitstream_read(&bs, 11, &val);
        ASSERT_EQ(val, (uint32_t)(172 + i * 100));
    }
}

/* ---------- overflow ---------- */

TEST(test_write_overflow)
{
    uint8_t buf[1] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 1);
    ASSERT_EQ(bitstream_write(&bs, 0xFF, 8), BITSTREAM_OK);
    ASSERT_EQ(bitstream_write(&bs, 1, 1), BITSTREAM_ERR_OVERFLOW);
}

TEST(test_read_overflow)
{
    uint8_t buf[1] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 1);
    uint32_t val = 0;
    ASSERT_EQ(bitstream_read(&bs, 8, &val), BITSTREAM_OK);
    ASSERT_EQ(bitstream_read(&bs, 1, &val), BITSTREAM_ERR_OVERFLOW);
}

TEST(test_write_zero_bits)
{
    uint8_t buf[1] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 1);
    ASSERT_EQ(bitstream_write(&bs, 0, 0), BITSTREAM_ERR_PARAM);
}

TEST(test_write_33_bits)
{
    uint8_t buf[8] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 8);
    ASSERT_EQ(bitstream_write(&bs, 0, 33), BITSTREAM_ERR_PARAM);
}

/* ---------- seek / tell / reset ---------- */

TEST(test_seek)
{
    uint8_t buf[4] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 4);
    bitstream_write(&bs, 0xAA, 8);
    bitstream_write(&bs, 0xBB, 8);
    ASSERT_EQ(bitstream_seek(&bs, 8), BITSTREAM_OK);
    uint32_t val = 0;
    bitstream_read(&bs, 8, &val);
    ASSERT_EQ(val, 0xBBu);
}

TEST(test_seek_overflow)
{
    uint8_t buf[1] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 1);
    ASSERT_EQ(bitstream_seek(&bs, 9), BITSTREAM_ERR_OVERFLOW);
}

TEST(test_reset)
{
    uint8_t buf[4] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 4);
    bitstream_write(&bs, 0xFF, 8);
    ASSERT_EQ(bitstream_tell(&bs), 8u);
    bitstream_reset(&bs);
    ASSERT_EQ(bitstream_tell(&bs), 0u);
}

TEST(test_reset_null)
{
    ASSERT_EQ(bitstream_reset(NULL), BITSTREAM_ERR_PARAM);
}

TEST(test_tell_null)
{
    ASSERT_EQ(bitstream_tell(NULL), 0u);
}

/* ---------- read null ---------- */

TEST(test_read_null)
{
    uint8_t buf[1] = {0};
    bitstream_t bs;
    bitstream_init(&bs, buf, 1);
    ASSERT_EQ(bitstream_read(&bs, 1, NULL), BITSTREAM_ERR_PARAM);
    ASSERT_EQ(bitstream_read(NULL, 1, &(uint32_t){0}), BITSTREAM_ERR_PARAM);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)BITSTREAM_OK, 0);
    ASSERT_EQ((int)BITSTREAM_ERR_PARAM, -1);
    ASSERT_EQ((int)BITSTREAM_ERR_OVERFLOW, -2);
}

TEST_MAIN("bitstream")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_write_read_8bit);
    RUN(test_write_read_11bit);
    RUN(test_write_read_1bit);
    RUN(test_write_read_32bit);
    RUN(test_sequential_writes);
    RUN(test_16_channels_11bit);
    RUN(test_write_overflow);
    RUN(test_read_overflow);
    RUN(test_write_zero_bits);
    RUN(test_write_33_bits);
    RUN(test_seek);
    RUN(test_seek_overflow);
    RUN(test_reset);
    RUN(test_reset_null);
    RUN(test_tell_null);
    RUN(test_read_null);
    RUN(test_error_codes);
TEST_MAIN_END()
