/**
 * @file    test_main.c
 * @brief   C unit tests for crc16 wheel.
 *
 * Returns 0 on all-pass, non-zero on first failure (pytest checks exit code).
 * Each TEST() prints PASS/FAIL line for human + machine readability.
 */

#include "crc16.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_failures = 0;

#define TEST(name, cond) do {                                          \
    int _ok = (cond);                                                  \
    printf("[%s] %s\n", _ok ? "PASS" : "FAIL", name);                  \
    if (!_ok) g_failures++;                                            \
} while (0)

static int test_empty_buffer(void)
{
    /* Empty input must return init value 0xFFFF (no bytes absorbed). */
    TEST("empty buffer returns 0xFFFF", crc16_modbus(NULL, 0) == 0xFFFFu);
    TEST("empty with non-NULL pointer",
         crc16_modbus((const uint8_t *)"unused", 0) == 0xFFFFu);
    return 0;
}

static int test_canonical_vector(void)
{
    /* "123456789" — canonical CRC16-MODBUS check value. Cited by every
     * Modbus RTU reference and online CRC calculators. */
    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_modbus(data, 9);
    TEST("123456789 -> 0x4B37", crc == 0x4B37u);
    return 0;
}

static int test_modbus_rtu_frame(void)
{
    /* Modbus RTU: slave 1, function 03 (read holding regs), start addr 0,
     * count 10. Expected CRC: low=0xC5, high=0xCD → 16-bit 0xCDC5. */
    const uint8_t frame[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A };
    uint16_t crc = crc16_modbus(frame, sizeof(frame));
    TEST("modbus rtu read-10-regs frame -> 0xCDC5", crc == 0xCDC5u);
    return 0;
}

static int test_single_byte(void)
{
    /* Single byte 0x00: init 0xFFFF, table[0xFF & 0x00] = table[0xFF] (0x4040) */
    const uint8_t b0 = 0x00;
    uint16_t crc = crc16_modbus(&b0, 1);
    TEST("single 0x00 -> 0x40BF", crc == 0x40BFu);
    /* Single byte 0xFF */
    const uint8_t bff = 0xFF;
    crc = crc16_modbus(&bff, 1);
    TEST("single 0xFF -> 0x00FF", crc == 0x00FFu);
    return 0;
}

static int test_step_equivalence(void)
{
    /* Streaming step API must match bulk API for identical input. */
    const uint8_t data[] = "The quick brown fox jumps over the lazy dog";
    size_t len = strlen((const char *)data);

    uint16_t bulk = crc16_modbus(data, len);

    uint16_t step = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < len; i++) {
        step = crc16_modbus_step(step, data[i]);
    }
    TEST("step API equals bulk API", step == bulk);
    return 0;
}

static int test_step_chunked(void)
{
    /* Caller may split the input across multiple calls; all must agree. */
    const uint8_t data[] = "aaaaaaaaaa";  /* 10 'a' */

    uint16_t bulk = crc16_modbus(data, 10);

    /* Split as 3 + 4 + 3 chunks */
    uint16_t step = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < 3;  i++) step = crc16_modbus_step(step, data[i]);
    for (size_t i = 3; i < 7;  i++) step = crc16_modbus_step(step, data[i]);
    for (size_t i = 7; i < 10; i++) step = crc16_modbus_step(step, data[i]);
    TEST("chunked step matches bulk", step == bulk);
    return 0;
}

static int test_flybox_compatibility(void)
{
    /* Regression: flybox telemetry frames are 36 bytes, CRC over first 34.
     * Construct a known frame and verify the CRC matches what the production
     * frame_pack.c produced before refactor. Using a deterministic pattern
     * for repeatability. */
    uint8_t frame34[34];
    for (size_t i = 0; i < 34; i++) frame34[i] = (uint8_t)(i * 7 + 3);

    uint16_t crc = crc16_modbus(frame34, 34);
    /* Computed once with the production frame_pack.c table; if this number
     * ever changes we have a real regression in the CRC core. */
    TEST("flybox 34-byte deterministic frame CRC stable", crc == 0xA655u);
    return 0;
}

static int test_all_zeros_buffer(void)
{
    uint8_t buf[256];
    memset(buf, 0x00, sizeof(buf));
    uint16_t crc = crc16_modbus(buf, sizeof(buf));
    TEST("256x 0x00 differs from init", crc != CRC16_MODBUS_INIT);
    TEST("256x 0x00 differs from single 0x00", crc != 0x40BFu);

    uint16_t step = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < sizeof(buf); i++)
        step = crc16_modbus_step(step, buf[i]);
    TEST("256x 0x00 step matches bulk", step == crc);
    return 0;
}

static int test_all_ones_buffer(void)
{
    uint8_t buf[256];
    memset(buf, 0xFF, sizeof(buf));
    uint16_t crc = crc16_modbus(buf, sizeof(buf));
    TEST("256x 0xFF differs from init", crc != CRC16_MODBUS_INIT);

    uint8_t zeros[256];
    memset(zeros, 0x00, sizeof(zeros));
    TEST("256x 0xFF differs from 256x 0x00",
         crc != crc16_modbus(zeros, sizeof(zeros)));
    return 0;
}

static int test_single_byte_all_distinct(void)
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
    TEST("all 256 single-byte CRCs distinct", collisions == 0);
    return 0;
}

static int test_crc_residue_zero(void)
{
    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_modbus(data, 9);
    uint8_t extended[11];
    memcpy(extended, data, 9);
    extended[9]  = (uint8_t)(crc & 0xFF);
    extended[10] = (uint8_t)(crc >> 8);
    uint16_t residue = crc16_modbus(extended, 11);
    TEST("CRC residue is zero (Modbus self-check)", residue == 0x0000u);
    return 0;
}

static int test_long_buffer_step(void)
{
    uint8_t buf[1024];
    for (size_t i = 0; i < sizeof(buf); i++)
        buf[i] = (uint8_t)(i * 13 + 7);

    uint16_t bulk = crc16_modbus(buf, sizeof(buf));

    uint16_t step = CRC16_MODBUS_INIT;
    for (size_t i = 0; i < sizeof(buf); i++)
        step = crc16_modbus_step(step, buf[i]);
    TEST("1024-byte step matches bulk", step == bulk);
    return 0;
}

int main(void)
{
    printf("=== crc16 wheel test suite ===\n");
    test_empty_buffer();
    test_canonical_vector();
    test_modbus_rtu_frame();
    test_single_byte();
    test_step_equivalence();
    test_step_chunked();
    test_flybox_compatibility();
    test_all_zeros_buffer();
    test_all_ones_buffer();
    test_single_byte_all_distinct();
    test_crc_residue_zero();
    test_long_buffer_step();

    printf("\n=== %d failures ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
