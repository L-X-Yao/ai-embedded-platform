// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "byte_buffer.h"
static byte_buffer_t bb;
static uint8_t storage[64];

static void setup(void)
{
    byte_buffer_init(&bb, storage, sizeof(storage));
}

TEST(test_init)
{
    setup();
    ASSERT_EQ(byte_buffer_readable(&bb), 0);
    ASSERT_EQ(byte_buffer_writable(&bb), 64);
    ASSERT_EQ(byte_buffer_written(&bb), 0);
}

TEST(test_null_safety)
{
    ASSERT_EQ(byte_buffer_init(NULL, storage, 64), BYTE_BUFFER_ERR_PARAM);
    ASSERT_EQ(byte_buffer_init(&bb, NULL, 64), BYTE_BUFFER_ERR_PARAM);
    ASSERT_EQ(byte_buffer_init(&bb, storage, 0), BYTE_BUFFER_ERR_PARAM);
    ASSERT_EQ(byte_buffer_reset(NULL), BYTE_BUFFER_ERR_PARAM);
    ASSERT_EQ(byte_buffer_put_u8(NULL, 0), BYTE_BUFFER_ERR_PARAM);
    uint8_t v8;
    ASSERT_EQ(byte_buffer_get_u8(NULL, &v8), BYTE_BUFFER_ERR_PARAM);
    setup();
    ASSERT_EQ(byte_buffer_get_u8(&bb, NULL), BYTE_BUFFER_ERR_PARAM);
    ASSERT_EQ(byte_buffer_readable(NULL), 0);
    ASSERT_EQ(byte_buffer_writable(NULL), 0);
    ASSERT_TRUE(byte_buffer_read_ptr(NULL) == NULL);
    ASSERT_EQ(byte_buffer_compact(NULL), BYTE_BUFFER_ERR_PARAM);
}

TEST(test_put_get_u8)
{
    setup();
    ASSERT_EQ(byte_buffer_put_u8(&bb, 0xAB), BYTE_BUFFER_OK);
    ASSERT_EQ(byte_buffer_readable(&bb), 1);

    uint8_t val;
    ASSERT_EQ(byte_buffer_get_u8(&bb, &val), BYTE_BUFFER_OK);
    ASSERT_EQ(val, 0xAB);
    ASSERT_EQ(byte_buffer_readable(&bb), 0);
}

TEST(test_put_get_u16)
{
    setup();
    ASSERT_EQ(byte_buffer_put_u16_be(&bb, 0x1234), BYTE_BUFFER_OK);
    ASSERT_EQ(byte_buffer_readable(&bb), 2);
    ASSERT_EQ(storage[0], 0x12);
    ASSERT_EQ(storage[1], 0x34);

    uint16_t val;
    ASSERT_EQ(byte_buffer_get_u16_be(&bb, &val), BYTE_BUFFER_OK);
    ASSERT_EQ(val, 0x1234);
}

TEST(test_put_get_u32)
{
    setup();
    ASSERT_EQ(byte_buffer_put_u32_be(&bb, 0xDEADBEEF), BYTE_BUFFER_OK);
    ASSERT_EQ(byte_buffer_readable(&bb), 4);

    uint32_t val;
    ASSERT_EQ(byte_buffer_get_u32_be(&bb, &val), BYTE_BUFFER_OK);
    ASSERT_TRUE(val == 0xDEADBEEF);
}

TEST(test_put_get_bytes)
{
    setup();
    uint8_t src[] = { 1, 2, 3, 4, 5 };
    ASSERT_EQ(byte_buffer_put_bytes(&bb, src, 5), BYTE_BUFFER_OK);
    ASSERT_EQ(byte_buffer_readable(&bb), 5);

    uint8_t dst[5] = {0};
    ASSERT_EQ(byte_buffer_get_bytes(&bb, dst, 5), BYTE_BUFFER_OK);
    ASSERT_TRUE(memcmp(dst, src, 5) == 0);
}

TEST(test_overflow)
{
    uint8_t small_store[4];
    byte_buffer_t small;
    byte_buffer_init(&small, small_store, 4);

    ASSERT_EQ(byte_buffer_put_u32_be(&small, 0x11223344), BYTE_BUFFER_OK);
    ASSERT_EQ(byte_buffer_put_u8(&small, 0xFF), BYTE_BUFFER_ERR_FULL);
    ASSERT_EQ(byte_buffer_writable(&small), 0);
}

TEST(test_underflow)
{
    setup();
    uint8_t val;
    ASSERT_EQ(byte_buffer_get_u8(&bb, &val), BYTE_BUFFER_ERR_EMPTY);

    byte_buffer_put_u8(&bb, 0x01);
    uint16_t v16;
    ASSERT_EQ(byte_buffer_get_u16_be(&bb, &v16), BYTE_BUFFER_ERR_EMPTY);
}

TEST(test_reset)
{
    setup();
    byte_buffer_put_u8(&bb, 0x01);
    byte_buffer_put_u8(&bb, 0x02);
    byte_buffer_reset(&bb);
    ASSERT_EQ(byte_buffer_readable(&bb), 0);
    ASSERT_EQ(byte_buffer_writable(&bb), 64);
}

TEST(test_read_ptr)
{
    setup();
    byte_buffer_put_u8(&bb, 0xAA);
    byte_buffer_put_u8(&bb, 0xBB);
    const uint8_t *ptr = byte_buffer_read_ptr(&bb);
    ASSERT_TRUE(ptr != NULL);
    ASSERT_EQ(ptr[0], 0xAA);
    ASSERT_EQ(ptr[1], 0xBB);

    uint8_t v;
    byte_buffer_get_u8(&bb, &v);
    ptr = byte_buffer_read_ptr(&bb);
    ASSERT_EQ(ptr[0], 0xBB);
}

TEST(test_compact)
{
    setup();
    byte_buffer_put_u8(&bb, 0x01);
    byte_buffer_put_u8(&bb, 0x02);
    byte_buffer_put_u8(&bb, 0x03);

    uint8_t v;
    byte_buffer_get_u8(&bb, &v);
    byte_buffer_get_u8(&bb, &v);

    ASSERT_EQ(byte_buffer_readable(&bb), 1);
    ASSERT_EQ(byte_buffer_writable(&bb), 61);

    byte_buffer_compact(&bb);
    ASSERT_EQ(byte_buffer_readable(&bb), 1);
    ASSERT_EQ(byte_buffer_writable(&bb), 63);

    byte_buffer_get_u8(&bb, &v);
    ASSERT_EQ(v, 0x03);
}

TEST(test_mixed_types)
{
    setup();
    byte_buffer_put_u8(&bb, 0x01);
    byte_buffer_put_u16_be(&bb, 0x0203);
    byte_buffer_put_u32_be(&bb, 0x04050607);
    ASSERT_EQ(byte_buffer_readable(&bb), 7);

    uint8_t v8; uint16_t v16; uint32_t v32;
    byte_buffer_get_u8(&bb, &v8);
    byte_buffer_get_u16_be(&bb, &v16);
    byte_buffer_get_u32_be(&bb, &v32);
    ASSERT_EQ(v8, 0x01);
    ASSERT_EQ(v16, 0x0203);
    ASSERT_TRUE(v32 == 0x04050607);
}

TEST(test_written)
{
    setup();
    ASSERT_EQ(byte_buffer_written(&bb), 0);
    byte_buffer_put_u8(&bb, 0x01);
    byte_buffer_put_u8(&bb, 0x02);
    ASSERT_EQ(byte_buffer_written(&bb), 2);
    ASSERT_EQ(byte_buffer_written(NULL), 0);
}

TEST_MAIN("byte_buffer")
    RUN(test_init);
    RUN(test_null_safety);
    RUN(test_put_get_u8);
    RUN(test_put_get_u16);
    RUN(test_put_get_u32);
    RUN(test_put_get_bytes);
    RUN(test_overflow);
    RUN(test_underflow);
    RUN(test_reset);
    RUN(test_read_ptr);
    RUN(test_compact);
    RUN(test_mixed_types);
    RUN(test_written);
TEST_MAIN_END()

