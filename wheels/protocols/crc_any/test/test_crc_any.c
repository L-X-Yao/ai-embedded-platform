// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "crc_any.h"
#include <string.h>

/* Standard check string for CRC catalogue vectors: ASCII "123456789" */
static const uint8_t check_data[] = "123456789";
static const size_t  check_len    = 9;

/* ── CRC-16/MODBUS: check=0x4B37 ─────────────────────────────────── */

TEST(test_crc16_modbus_vector)
{
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(&CRC_ANY_CRC16_MODBUS, check_data, check_len, &crc),
              CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 0x4B37);
}

/* ── CRC-16/CCITT-FALSE: check=0x29B1 ────────────────────────────── */

TEST(test_crc16_ccitt_vector)
{
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(&CRC_ANY_CRC16_CCITT, check_data, check_len, &crc),
              CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 0x29B1);
}

/* ── CRC-32/ISO-HDLC (Ethernet/zlib): check=0xCBF43926 ───────────── */

TEST(test_crc32_iso_vector)
{
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(&CRC_ANY_CRC32_ISO, check_data, check_len, &crc),
              CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 0xCBF43926);
}

/* ── CRC-8/MAXIM (1-Wire): check=0xA1 ────────────────────────────── */

TEST(test_crc8_maxim_vector)
{
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(&CRC_ANY_CRC8_MAXIM, check_data, check_len, &crc),
              CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 0xA1);
}

/* ── Streaming (update) matches one-shot (compute) ────────────────── */

TEST(test_streaming_equals_bulk_crc32)
{
    crc_any_config_t cfg = CRC_ANY_CRC32_ISO;
    uint32_t bulk;
    crc_any_compute(&cfg, check_data, check_len, &bulk);

    /* Feed byte-by-byte */
    uint32_t crc = cfg.init;
    for (size_t i = 0; i < check_len; i++)
        crc = crc_any_update(&cfg, crc, &check_data[i], 1);
    /* Finalize via compute on empty (init=running crc) */
    crc_any_config_t fin = cfg;
    fin.init = crc;
    uint32_t streamed;
    crc_any_compute(&fin, NULL, 0, &streamed);

    ASSERT_EQ_HEX(streamed, bulk);
}

TEST(test_streaming_segmented_crc16)
{
    crc_any_config_t cfg = CRC_ANY_CRC16_MODBUS;
    uint32_t bulk;
    crc_any_compute(&cfg, check_data, check_len, &bulk);

    /* Feed in segments: 3 + 4 + 2 */
    uint32_t crc = cfg.init;
    crc = crc_any_update(&cfg, crc, check_data, 3);
    crc = crc_any_update(&cfg, crc, check_data + 3, 4);
    crc = crc_any_update(&cfg, crc, check_data + 7, 2);
    crc_any_config_t fin = cfg;
    fin.init = crc;
    uint32_t streamed;
    crc_any_compute(&fin, NULL, 0, &streamed);

    ASSERT_EQ_HEX(streamed, bulk);
}

/* ── Verify function ──────────────────────────────────────────────── */

TEST(test_verify_pass)
{
    ASSERT_TRUE(crc_any_verify(&CRC_ANY_CRC32_ISO, check_data, check_len,
                               0xCBF43926));
}

TEST(test_verify_fail)
{
    ASSERT_FALSE(crc_any_verify(&CRC_ANY_CRC32_ISO, check_data, check_len,
                                0xDEADBEEF));
}

/* ── Edge cases ───────────────────────────────────────────────────── */

TEST(test_empty_input)
{
    /* CRC-32 of empty data: init ^ xorout = 0xFFFFFFFF ^ 0xFFFFFFFF = 0 */
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(&CRC_ANY_CRC32_ISO, NULL, 0, &crc), CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 0x00000000);
}

TEST(test_single_zero_byte_crc8)
{
    uint8_t b = 0x00;
    uint32_t crc;
    crc_any_config_t cfg = CRC_ANY_CRC8_MAXIM;
    ASSERT_EQ(crc_any_compute(&cfg, &b, 1, &crc), CRC_ANY_OK);
    /* CRC-8/MAXIM of 0x00: poly 0x31, init 0, reflected — yields 0x00 */
    ASSERT_EQ_HEX(crc, 0x00);
}

TEST(test_null_cfg_returns_err)
{
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(NULL, check_data, check_len, &crc),
              CRC_ANY_ERR_PARAM);
}

TEST(test_null_out_returns_err)
{
    ASSERT_EQ(crc_any_compute(&CRC_ANY_CRC16_MODBUS, check_data, check_len, NULL),
              CRC_ANY_ERR_PARAM);
}

TEST(test_width_zero_returns_err)
{
    crc_any_config_t bad = CRC_ANY_CRC16_MODBUS;
    bad.width = 0;
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(&bad, check_data, check_len, &crc),
              CRC_ANY_ERR_PARAM);
}

/* ── Additional catalogue vector: CRC-16/XMODEM (non-reflected) ─── */

TEST(test_crc16_xmodem_vector)
{
    /* CRC-16/XMODEM: width=16, poly=0x1021, init=0, refin=false,
       refout=false, xorout=0, check=0x31C3 */
    crc_any_config_t cfg = {
        .width = 16, .poly = 0x1021, .init = 0x0000,
        .refin = false, .refout = false, .xorout = 0x0000
    };
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(&cfg, check_data, check_len, &crc), CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 0x31C3);
}

/* ── Additional catalogue vector: CRC-8/ROHC (reflected) ─────────── */

TEST(test_crc8_rohc_vector)
{
    /* CRC-8/ROHC: width=8, poly=0x07, init=0xFF, refin=true,
       refout=true, xorout=0x00, check=0xD0 */
    crc_any_config_t cfg = {
        .width = 8, .poly = 0x07, .init = 0xFF,
        .refin = true, .refout = true, .xorout = 0x00
    };
    uint32_t crc;
    ASSERT_EQ(crc_any_compute(&cfg, check_data, check_len, &crc), CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 0xD0);
}

TEST_MAIN("crc_any")
    RUN(test_crc16_modbus_vector);
    RUN(test_crc16_ccitt_vector);
    RUN(test_crc32_iso_vector);
    RUN(test_crc8_maxim_vector);
    RUN(test_streaming_equals_bulk_crc32);
    RUN(test_streaming_segmented_crc16);
    RUN(test_verify_pass);
    RUN(test_verify_fail);
    RUN(test_empty_input);
    RUN(test_single_zero_byte_crc8);
    RUN(test_null_cfg_returns_err);
    RUN(test_null_out_returns_err);
    RUN(test_width_zero_returns_err);
    RUN(test_crc16_xmodem_vector);
    RUN(test_crc8_rohc_vector);
TEST_MAIN_END()
