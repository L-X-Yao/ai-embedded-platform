#include <stdio.h>
#include <string.h>
#include "../byte_buffer.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((int)(a) == (int)(b), msg)

static byte_buffer_t bb;
static uint8_t storage[64];

static void setup(void)
{
    byte_buffer_init(&bb, storage, sizeof(storage));
}

static void test_init(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(byte_buffer_readable(&bb), 0, "empty after init");
    ASSERT_EQ(byte_buffer_writable(&bb), 64, "full capacity writable");
    ASSERT_EQ(byte_buffer_written(&bb), 0, "nothing written");
}

static void test_null_safety(void)
{
    printf("%s\n", __func__);
    ASSERT_EQ(byte_buffer_init(NULL, storage, 64), BYTE_BUFFER_ERR_PARAM, "init null buf");
    ASSERT_EQ(byte_buffer_init(&bb, NULL, 64), BYTE_BUFFER_ERR_PARAM, "init null storage");
    ASSERT_EQ(byte_buffer_init(&bb, storage, 0), BYTE_BUFFER_ERR_PARAM, "init zero cap");
    ASSERT_EQ(byte_buffer_reset(NULL), BYTE_BUFFER_ERR_PARAM, "reset null");
    ASSERT_EQ(byte_buffer_put_u8(NULL, 0), BYTE_BUFFER_ERR_PARAM, "put_u8 null");
    uint8_t v8;
    ASSERT_EQ(byte_buffer_get_u8(NULL, &v8), BYTE_BUFFER_ERR_PARAM, "get_u8 null buf");
    setup();
    ASSERT_EQ(byte_buffer_get_u8(&bb, NULL), BYTE_BUFFER_ERR_PARAM, "get_u8 null val");
    ASSERT_EQ(byte_buffer_readable(NULL), 0, "readable null");
    ASSERT_EQ(byte_buffer_writable(NULL), 0, "writable null");
    ASSERT(byte_buffer_read_ptr(NULL) == NULL, "read_ptr null");
    ASSERT_EQ(byte_buffer_compact(NULL), BYTE_BUFFER_ERR_PARAM, "compact null");
}

static void test_put_get_u8(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(byte_buffer_put_u8(&bb, 0xAB), BYTE_BUFFER_OK, "put ok");
    ASSERT_EQ(byte_buffer_readable(&bb), 1, "1 readable");

    uint8_t val;
    ASSERT_EQ(byte_buffer_get_u8(&bb, &val), BYTE_BUFFER_OK, "get ok");
    ASSERT_EQ(val, 0xAB, "value matches");
    ASSERT_EQ(byte_buffer_readable(&bb), 0, "0 readable after get");
}

static void test_put_get_u16(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(byte_buffer_put_u16_be(&bb, 0x1234), BYTE_BUFFER_OK, "put u16");
    ASSERT_EQ(byte_buffer_readable(&bb), 2, "2 readable");
    ASSERT_EQ(storage[0], 0x12, "MSB first");
    ASSERT_EQ(storage[1], 0x34, "LSB second");

    uint16_t val;
    ASSERT_EQ(byte_buffer_get_u16_be(&bb, &val), BYTE_BUFFER_OK, "get u16");
    ASSERT_EQ(val, 0x1234, "value matches");
}

static void test_put_get_u32(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(byte_buffer_put_u32_be(&bb, 0xDEADBEEF), BYTE_BUFFER_OK, "put u32");
    ASSERT_EQ(byte_buffer_readable(&bb), 4, "4 readable");

    uint32_t val;
    ASSERT_EQ(byte_buffer_get_u32_be(&bb, &val), BYTE_BUFFER_OK, "get u32");
    ASSERT(val == 0xDEADBEEF, "value matches");
}

static void test_put_get_bytes(void)
{
    printf("%s\n", __func__);
    setup();
    uint8_t src[] = { 1, 2, 3, 4, 5 };
    ASSERT_EQ(byte_buffer_put_bytes(&bb, src, 5), BYTE_BUFFER_OK, "put bytes");
    ASSERT_EQ(byte_buffer_readable(&bb), 5, "5 readable");

    uint8_t dst[5] = {0};
    ASSERT_EQ(byte_buffer_get_bytes(&bb, dst, 5), BYTE_BUFFER_OK, "get bytes");
    ASSERT(memcmp(dst, src, 5) == 0, "bytes match");
}

static void test_overflow(void)
{
    printf("%s\n", __func__);
    uint8_t small_store[4];
    byte_buffer_t small;
    byte_buffer_init(&small, small_store, 4);

    ASSERT_EQ(byte_buffer_put_u32_be(&small, 0x11223344), BYTE_BUFFER_OK, "put 4 bytes");
    ASSERT_EQ(byte_buffer_put_u8(&small, 0xFF), BYTE_BUFFER_ERR_FULL, "5th byte fails");
    ASSERT_EQ(byte_buffer_writable(&small), 0, "0 writable");
}

static void test_underflow(void)
{
    printf("%s\n", __func__);
    setup();
    uint8_t val;
    ASSERT_EQ(byte_buffer_get_u8(&bb, &val), BYTE_BUFFER_ERR_EMPTY, "get from empty");

    byte_buffer_put_u8(&bb, 0x01);
    uint16_t v16;
    ASSERT_EQ(byte_buffer_get_u16_be(&bb, &v16), BYTE_BUFFER_ERR_EMPTY, "get u16 from 1 byte");
}

static void test_reset(void)
{
    printf("%s\n", __func__);
    setup();
    byte_buffer_put_u8(&bb, 0x01);
    byte_buffer_put_u8(&bb, 0x02);
    byte_buffer_reset(&bb);
    ASSERT_EQ(byte_buffer_readable(&bb), 0, "empty after reset");
    ASSERT_EQ(byte_buffer_writable(&bb), 64, "full capacity after reset");
}

static void test_read_ptr(void)
{
    printf("%s\n", __func__);
    setup();
    byte_buffer_put_u8(&bb, 0xAA);
    byte_buffer_put_u8(&bb, 0xBB);
    const uint8_t *ptr = byte_buffer_read_ptr(&bb);
    ASSERT(ptr != NULL, "ptr not null");
    ASSERT_EQ(ptr[0], 0xAA, "ptr[0]");
    ASSERT_EQ(ptr[1], 0xBB, "ptr[1]");

    uint8_t v;
    byte_buffer_get_u8(&bb, &v);
    ptr = byte_buffer_read_ptr(&bb);
    ASSERT_EQ(ptr[0], 0xBB, "ptr advances after read");
}

static void test_compact(void)
{
    printf("%s\n", __func__);
    setup();
    byte_buffer_put_u8(&bb, 0x01);
    byte_buffer_put_u8(&bb, 0x02);
    byte_buffer_put_u8(&bb, 0x03);

    uint8_t v;
    byte_buffer_get_u8(&bb, &v);
    byte_buffer_get_u8(&bb, &v);

    ASSERT_EQ(byte_buffer_readable(&bb), 1, "1 unread before compact");
    ASSERT_EQ(byte_buffer_writable(&bb), 61, "61 writable before compact");

    byte_buffer_compact(&bb);
    ASSERT_EQ(byte_buffer_readable(&bb), 1, "1 unread after compact");
    ASSERT_EQ(byte_buffer_writable(&bb), 63, "63 writable after compact");

    byte_buffer_get_u8(&bb, &v);
    ASSERT_EQ(v, 0x03, "compacted data intact");
}

static void test_mixed_types(void)
{
    printf("%s\n", __func__);
    setup();
    byte_buffer_put_u8(&bb, 0x01);
    byte_buffer_put_u16_be(&bb, 0x0203);
    byte_buffer_put_u32_be(&bb, 0x04050607);
    ASSERT_EQ(byte_buffer_readable(&bb), 7, "7 bytes total");

    uint8_t v8; uint16_t v16; uint32_t v32;
    byte_buffer_get_u8(&bb, &v8);
    byte_buffer_get_u16_be(&bb, &v16);
    byte_buffer_get_u32_be(&bb, &v32);
    ASSERT_EQ(v8, 0x01, "u8");
    ASSERT_EQ(v16, 0x0203, "u16");
    ASSERT(v32 == 0x04050607, "u32");
}

static void test_written(void)
{
    printf("%s\n", __func__);
    setup();
    ASSERT_EQ(byte_buffer_written(&bb), 0, "0 written initially");
    byte_buffer_put_u8(&bb, 0x01);
    byte_buffer_put_u8(&bb, 0x02);
    ASSERT_EQ(byte_buffer_written(&bb), 2, "2 written");
    ASSERT_EQ(byte_buffer_written(NULL), 0, "written null");
}

int main(void)
{
    printf("test_byte_buffer\n");
    test_init();
    test_null_safety();
    test_put_get_u8();
    test_put_get_u16();
    test_put_get_u32();
    test_put_get_bytes();
    test_overflow();
    test_underflow();
    test_reset();
    test_read_ptr();
    test_compact();
    test_mixed_types();
    test_written();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
