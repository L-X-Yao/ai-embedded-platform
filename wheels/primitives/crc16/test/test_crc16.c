// SPDX-License-Identifier: MIT
#include "test_harness.h"
/**
 * @file    test_crc16.c
 * @brief   C unit tests for crc16 wheel.
 *
 * Returns 0 on all-pass, non-zero on first failure (pytest checks exit code).
 * Each TEST() prints PASS/FAIL line for human + machine readability.
 */

#include "crc16.h"
#include <string.h>
#include <stdint.h>
TEST(test_empty_buffer)
{
    /* Empty input must return init value 0xFFFF (no bytes absorbed). */
    ASSERT_TRUE(crc16_modbus(NULL, 0) == 0xFFFFu);
    ASSERT_TRUE(crc16_modbus((const uint8_t *)"unused", 0) == 0xFFFFu);
}

TEST(test_canonical_vector)
{
    /* "123456789" — canonical CRC16-MODBUS check value. Cited by every
     * Modbus RTU reference and online CRC calculators. */
    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_modbus(data, 9);
    ASSERT_TRUE(crc == 0x4B37u);
}

TEST(test_modbus_rtu_frame)
{
    /* Modbus RTU: slave 1, function 03 (read holding regs), start addr 0,
     * count 10. Expected CRC: low=0xC5, high=0xCD → 16-bit 0xCDC5. */
    const uint8_t frame[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A };
    uint16_t crc = crc16_modbus(frame, sizeof(frame));
    ASSERT_TRUE(crc == 0xCDC5u);
}

TEST(test_single_byte)
{
    /* Single byte 0x00: init 0xFFFF, table[0xFF & 0x00] = table[0xFF] (0x4040) */
    const uint8_t b0 = 0x00;
    uint16_t crc = crc16_modbus(&b0, 1);
    ASSERT_TRUE(crc == 0x40BFu);
    /* Single byte 0xFF */
    const uint8_t bff = 0xFF;
    crc = crc16_modbus(&bff, 1);
    ASSERT_TRUE(crc == 0x00FFu);
}

TEST(test_step_equivalence)
{
    /* Streaming step API must match bulk API for identical input. */
    const uint8_t data[] = "The quick brown fox jumps over the lazy dog";
    size_t len = strlen((const char *)data);

    uint16_t bulk = crc16_modbus(data, len);

    uint16_t step = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < len; i++) {
        step = crc16_modbus_step(step, data[i]);
    }
    ASSERT_TRUE(step == bulk);
}

TEST(test_step_chunked)
{
    /* Caller may split the input across multiple calls; all must agree. */
    const uint8_t data[] = "aaaaaaaaaa";  /* 10 'a' */

    uint16_t bulk = crc16_modbus(data, 10);

    /* Split as 3 + 4 + 3 chunks */
    uint16_t step = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < 3;  i++) step = crc16_modbus_step(step, data[i]);
    for (size_t i = 3; i < 7;  i++) step = crc16_modbus_step(step, data[i]);
    for (size_t i = 7; i < 10; i++) step = crc16_modbus_step(step, data[i]);
    ASSERT_TRUE(step == bulk);
}

TEST(test_fixed_pattern_regression)
{
    /* A deterministic 34-byte pattern: if this number ever changes we have a
     * real regression in the CRC core. */
    uint8_t pattern[34];
    for (size_t i = 0; i < 34; i++) pattern[i] = (uint8_t)(i * 7 + 3);

    uint16_t crc = crc16_modbus(pattern, 34);
    ASSERT_TRUE(crc == 0xA655u);
}

TEST(test_all_zeros_buffer)
{
    uint8_t buf[256];
    memset(buf, 0x00, sizeof(buf));
    uint16_t crc = crc16_modbus(buf, sizeof(buf));
    ASSERT_TRUE(crc != CRC16_MODBUS_INIT);
    ASSERT_TRUE(crc != 0x40BFu);

    uint16_t step = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < sizeof(buf); i++)
        step = crc16_modbus_step(step, buf[i]);
    ASSERT_TRUE(step == crc);
}

TEST(test_all_ones_buffer)
{
    uint8_t buf[256];
    memset(buf, 0xFF, sizeof(buf));
    uint16_t crc = crc16_modbus(buf, sizeof(buf));
    ASSERT_TRUE(crc != CRC16_MODBUS_INIT);

    uint8_t zeros[256];
    memset(zeros, 0x00, sizeof(zeros));
    ASSERT_TRUE(crc != crc16_modbus(zeros, sizeof(zeros)));
}

TEST(test_single_byte_all_distinct)
{
    uint16_t crcs[256];
    for (int i = 0; i < 256; i++)
        crcs[i] = crc16_modbus((const uint8_t *)&i, 1);

    int collisions = 0;
    for (int i = 0; i < 256; i++) {
        for (int j = i + 1; j < 256; j++) {
            if (crcs[i] == crcs[j]) collisions++;
        }
    }
    ASSERT_TRUE(collisions == 0);
}

TEST(test_crc_residue_zero)
{
    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_modbus(data, 9);
    uint8_t extended[11];
    memcpy(extended, data, 9);
    extended[9]  = (uint8_t)(crc & 0xFF);
    extended[10] = (uint8_t)(crc >> 8);
    uint16_t residue = crc16_modbus(extended, 11);
    ASSERT_TRUE(residue == 0x0000u);
}

TEST(test_long_buffer_step)
{
    uint8_t buf[1024];
    for (size_t i = 0; i < sizeof(buf); i++)
        buf[i] = (uint8_t)(i * 13 + 7);

    uint16_t bulk = crc16_modbus(buf, sizeof(buf));

    uint16_t step = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < sizeof(buf); i++)
        step = crc16_modbus_step(step, buf[i]);
    ASSERT_TRUE(step == bulk);
}

TEST_MAIN("crc16")
    RUN(test_empty_buffer);
    RUN(test_canonical_vector);
    RUN(test_modbus_rtu_frame);
    RUN(test_single_byte);
    RUN(test_step_equivalence);
    RUN(test_step_chunked);
    RUN(test_fixed_pattern_regression);
    RUN(test_all_zeros_buffer);
    RUN(test_all_ones_buffer);
    RUN(test_single_byte_all_distinct);
    RUN(test_crc_residue_zero);
    RUN(test_long_buffer_step);
TEST_MAIN_END()

