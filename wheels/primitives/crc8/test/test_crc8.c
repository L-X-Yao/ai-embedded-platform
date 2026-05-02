#include "test_harness.h"
#include "crc8.h"
#include <string.h>

/* ---- CRC8_DEFAULT / CRC8_SMBUS tests ----------------------------------- */

TEST(test_default_standard_vector)
{
    /* "123456789" standard CRC-8 check value (poly=0x07, init=0x00).
     * Reference: https://reveng.sourceforge.io/crc-catalogue/1-15.htm#crc.cat.crc-8 */
    const uint8_t data[] = "123456789";
    ASSERT_EQ_HEX(crc8_compute(&CRC8_DEFAULT, data, 9), 0xF4);
}

TEST(test_smbus_equals_default)
{
    /* CRC8_SMBUS and CRC8_DEFAULT are identical (poly=0x07, init=0x00). */
    const uint8_t data[] = "123456789";
    ASSERT_EQ_HEX(crc8_compute(&CRC8_SMBUS, data, 9),
                   crc8_compute(&CRC8_DEFAULT, data, 9));
}

/* ---- CRC8_MAXIM (1-Wire / Dallas) tests -------------------------------- */

TEST(test_maxim_standard_vector)
{
    /* "123456789" CRC-8/MAXIM check value.
     * Reference: reveng.sourceforge.io — CRC-8/MAXIM-DOW check=0xA1 */
    const uint8_t data[] = "123456789";
    ASSERT_EQ_HEX(crc8_compute(&CRC8_MAXIM, data, 9), 0xA1);
}

TEST(test_maxim_1wire_rom)
{
    /* 1-Wire ROM CRC example: compute CRC8/MAXIM over 7 bytes.
     * Verified against Python reference implementation. */
    const uint8_t rom[7] = { 0x28, 0xFF, 0x64, 0x1E, 0x00, 0x16, 0x02 };
    ASSERT_EQ_HEX(crc8_compute(&CRC8_MAXIM, rom, 7), 0x91);
}

/* ---- CRC8_SENSIRION (SHT30 / SI7021) tests ----------------------------- */

TEST(test_sensirion_standard_vector)
{
    /* "123456789" with CRC8_SENSIRION (poly=0x31, init=0xFF).
     * Verified against Python reference implementation. */
    const uint8_t data[] = "123456789";
    ASSERT_EQ_HEX(crc8_compute(&CRC8_SENSIRION, data, 9), 0xF7);
}

TEST(test_sensirion_sht30_example)
{
    /* SHT30 datasheet: measurement data 0xBE 0xEF, CRC = 0x92.
     * (Sensirion CRC-8: poly=0x31, init=0xFF) */
    const uint8_t data[] = { 0xBE, 0xEF };
    ASSERT_EQ_HEX(crc8_compute(&CRC8_SENSIRION, data, 2), 0x92);
}

/* ---- CRC8_ROHC tests --------------------------------------------------- */

TEST(test_rohc_standard_vector)
{
    /* "123456789" CRC-8/ROHC check value = 0xD0.
     * Reference: reveng.sourceforge.io — CRC-8/ROHC */
    const uint8_t data[] = "123456789";
    ASSERT_EQ_HEX(crc8_compute(&CRC8_ROHC, data, 9), 0xD0);
}

/* ---- Verify pass / fail ------------------------------------------------ */

TEST(test_verify_pass)
{
    const uint8_t data[] = "123456789";
    uint8_t expected = crc8_compute(&CRC8_DEFAULT, data, 9);
    ASSERT_TRUE(crc8_verify(&CRC8_DEFAULT, data, 9, expected));
}

TEST(test_verify_fail)
{
    const uint8_t data[] = "123456789";
    uint8_t expected = crc8_compute(&CRC8_DEFAULT, data, 9);
    ASSERT_FALSE(crc8_verify(&CRC8_DEFAULT, data, 9, (uint8_t)(expected ^ 0x01)));
}

/* ---- Streaming (update) tests ------------------------------------------ */

TEST(test_update_streaming_split)
{
    /* Streaming via update() split across 3 chunks must match bulk compute
     * for non-reflected config (CRC8_DEFAULT). */
    const uint8_t data[] = "123456789";
    uint8_t bulk = crc8_compute(&CRC8_DEFAULT, data, 9);

    uint8_t crc = CRC8_DEFAULT.init;
    crc = crc8_update(&CRC8_DEFAULT, crc, data,     3);
    crc = crc8_update(&CRC8_DEFAULT, crc, data + 3, 4);
    crc = crc8_update(&CRC8_DEFAULT, crc, data + 7, 2);
    /* Non-reflected, no xor_out — raw accumulator equals final CRC. */
    ASSERT_EQ_HEX(crc, bulk);
}

TEST(test_update_byte_by_byte)
{
    /* Byte-by-byte streaming must match bulk for non-reflected config. */
    const uint8_t data[] = "The quick brown fox";
    size_t len = strlen((const char *)data);
    uint8_t bulk = crc8_compute(&CRC8_SENSIRION, data, len);

    uint8_t crc = CRC8_SENSIRION.init;
    for (size_t i = 0; i < len; i++)
        crc = crc8_update(&CRC8_SENSIRION, crc, &data[i], 1);
    ASSERT_EQ_HEX(crc, bulk);
}

/* ---- Edge cases -------------------------------------------------------- */

TEST(test_empty_data)
{
    /* Empty input: CRC equals init ^ xor_out (with possible reflect). */
    ASSERT_EQ_HEX(crc8_compute(&CRC8_DEFAULT, NULL, 0), 0x00);
    ASSERT_EQ_HEX(crc8_compute(&CRC8_SENSIRION, NULL, 0), 0xFF);
    const uint8_t dummy[] = { 0xAA };
    ASSERT_EQ_HEX(crc8_compute(&CRC8_DEFAULT, dummy, 0), 0x00);
}

TEST(test_single_byte)
{
    uint8_t b = 0x00;
    /* CRC8_DEFAULT: init=0, XOR 0x00 into register, shift 8 times with poly=0x07 */
    uint8_t crc = crc8_compute(&CRC8_DEFAULT, &b, 1);
    ASSERT_EQ_HEX(crc, 0x00);

    b = 0x01;
    crc = crc8_compute(&CRC8_DEFAULT, &b, 1);
    /* 0x01 XOR 0x00 = 0x01, after 8 shifts with poly 0x07 = 0x07 */
    ASSERT_EQ_HEX(crc, 0x07);
}

TEST(test_null_config)
{
    /* NULL config must not crash. crc8_compute returns 0,
     * crc8_verify returns false (defensive). */
    const uint8_t data[] = { 0x01 };
    ASSERT_EQ_HEX(crc8_compute(NULL, data, 1), 0x00);
    /* verify with NULL config always returns false for safety. */
    ASSERT_FALSE(crc8_verify(NULL, data, 1, 0x42));
}

/* ---- Custom config test ------------------------------------------------ */

TEST(test_custom_config)
{
    /* Custom: poly=0x9B, init=0x00, refin/refout (CRC-8/WCDMA).
     * Check value 0x25 per reveng catalogue. */
    const crc8_config_t custom = { 0x9B, 0x00, true, true, 0x00 };
    const uint8_t data[] = "123456789";
    ASSERT_EQ_HEX(crc8_compute(&custom, data, 9), 0x25);
}

/* ---- Reflected vs non-reflected ---------------------------------------- */

TEST(test_reflected_vs_nonreflected)
{
    /* Same poly, same init — reflected I/O must produce different result. */
    const crc8_config_t non_ref = { 0x31, 0x00, false, false, 0x00 };
    const crc8_config_t ref     = { 0x31, 0x00, true,  true,  0x00 };
    const uint8_t data[] = "123456789";
    uint8_t crc_nr = crc8_compute(&non_ref, data, 9);
    uint8_t crc_r  = crc8_compute(&ref, data, 9);
    /* They must differ (unless astronomically unlikely collision). */
    ASSERT_TRUE(crc_nr != crc_r);
}

/* ---- All-zeros / all-ones buffers -------------------------------------- */

TEST(test_all_zeros_all_ones)
{
    uint8_t zeros[64], ones[64];
    memset(zeros, 0x00, sizeof(zeros));
    memset(ones,  0xFF, sizeof(ones));

    uint8_t crc_z = crc8_compute(&CRC8_DEFAULT, zeros, 64);
    uint8_t crc_o = crc8_compute(&CRC8_DEFAULT, ones, 64);
    /* Must produce different CRCs for different data. */
    ASSERT_TRUE(crc_z != crc_o);
}

TEST_MAIN("crc8")
    RUN(test_default_standard_vector);
    RUN(test_smbus_equals_default);
    RUN(test_maxim_standard_vector);
    RUN(test_maxim_1wire_rom);
    RUN(test_sensirion_standard_vector);
    RUN(test_sensirion_sht30_example);
    RUN(test_rohc_standard_vector);
    RUN(test_verify_pass);
    RUN(test_verify_fail);
    RUN(test_update_streaming_split);
    RUN(test_update_byte_by_byte);
    RUN(test_empty_data);
    RUN(test_single_byte);
    RUN(test_null_config);
    RUN(test_custom_config);
    RUN(test_reflected_vs_nonreflected);
    RUN(test_all_zeros_all_ones);
TEST_MAIN_END()
