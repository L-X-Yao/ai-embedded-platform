#include "test_harness.h"
#include "crc32.h"
#include <string.h>
#include <stdlib.h>

TEST(test_standard_vector)
{
    const uint8_t data[] = "123456789";
    ASSERT_EQ(crc32_compute(data, 9), 0xCBF43926u);
}

TEST(test_empty)
{
    ASSERT_EQ(crc32_compute(NULL, 0), 0x00000000u);
}

TEST(test_single_byte_zero)
{
    uint8_t b = 0x00;
    ASSERT_EQ(crc32_compute(&b, 1), 0xD202EF8Du);
}

TEST(test_streaming_equals_bulk)
{
    const uint8_t data[] = "123456789";
    uint32_t crc = CRC32_INIT;
    for (int i = 0; i < 9; i++)
        crc = crc32_update(crc, &data[i], 1);
    crc = crc32_finalize(crc);
    ASSERT_EQ(crc, crc32_compute(data, 9));
}

TEST(test_segmented_streaming)
{
    const uint8_t data[] = "123456789";
    uint32_t crc = CRC32_INIT;
    crc = crc32_update(crc, data, 3);
    crc = crc32_update(crc, data + 3, 4);
    crc = crc32_update(crc, data + 7, 2);
    crc = crc32_finalize(crc);
    ASSERT_EQ(crc, 0xCBF43926u);
}

TEST(test_all_zeros)
{
    uint8_t buf[256];
    memset(buf, 0, sizeof(buf));
    uint32_t bulk = crc32_compute(buf, 256);
    uint32_t crc = CRC32_INIT;
    for (int i = 0; i < 256; i++)
        crc = crc32_update(crc, &buf[i], 1);
    crc = crc32_finalize(crc);
    ASSERT_EQ(bulk, crc);
}

TEST(test_all_ff)
{
    uint8_t buf[256];
    memset(buf, 0xFF, sizeof(buf));
    uint32_t bulk = crc32_compute(buf, 256);
    uint32_t crc = CRC32_INIT;
    for (int i = 0; i < 256; i++)
        crc = crc32_update(crc, &buf[i], 1);
    crc = crc32_finalize(crc);
    ASSERT_EQ(bulk, crc);
}

TEST(test_residue_check)
{
    const uint8_t data[] = "123456789";
    uint32_t crc = crc32_compute(data, 9);
    uint8_t frame[13];
    memcpy(frame, data, 9);
    frame[9]  = (uint8_t)(crc & 0xFF);
    frame[10] = (uint8_t)((crc >> 8) & 0xFF);
    frame[11] = (uint8_t)((crc >> 16) & 0xFF);
    frame[12] = (uint8_t)((crc >> 24) & 0xFF);
    ASSERT_EQ(crc32_compute(frame, 13), 0x2144DF1Cu);
}

TEST(test_known_string)
{
    const uint8_t data[] = "The quick brown fox jumps over the lazy dog";
    ASSERT_EQ(crc32_compute(data, 43), 0x414FA339u);
}

TEST(test_1024_byte_consistency)
{
    uint8_t buf[1024];
    for (int i = 0; i < 1024; i++)
        buf[i] = (uint8_t)(i & 0xFF);
    uint32_t bulk = crc32_compute(buf, 1024);
    uint32_t crc = CRC32_INIT;
    for (int i = 0; i < 1024; i++)
        crc = crc32_update(crc, &buf[i], 1);
    crc = crc32_finalize(crc);
    ASSERT_EQ(bulk, crc);
}

TEST_MAIN("crc32")
    RUN(test_standard_vector);
    RUN(test_empty);
    RUN(test_single_byte_zero);
    RUN(test_streaming_equals_bulk);
    RUN(test_segmented_streaming);
    RUN(test_all_zeros);
    RUN(test_all_ff);
    RUN(test_residue_check);
    RUN(test_known_string);
    RUN(test_1024_byte_consistency);
TEST_MAIN_END()
