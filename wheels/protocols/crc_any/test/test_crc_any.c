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

TEST(test_update_leaves_the_crc_alone_on_a_bad_width)
{
    static const uint8_t data[1] = { 0x31 };
    crc_any_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.width = 0;
    ASSERT_EQ(crc_any_update(&cfg, 0x1234u, data, 1), 0x1234u);
    cfg.width = 33;
    ASSERT_EQ(crc_any_update(&cfg, 0x1234u, data, 1), 0x1234u);
}

/* ── Widths below 8: every entry of the reveng catalogue ─────────── */

/* All parameters and check values are copied from
   https://reveng.sourceforge.io/crc-catalogue/1-15.htm */
static const struct {
    crc_any_config_t cfg;
    uint32_t         check;
} narrow_vectors[] = {
    /* CRC-3/GSM */
    { { 3, 0x3, 0x0, false, false, 0x7 }, 0x4 },
    /* CRC-3/ROHC */
    { { 3, 0x3, 0x7, true, true, 0x0 }, 0x6 },
    /* CRC-4/G-704 */
    { { 4, 0x3, 0x0, true, true, 0x0 }, 0x7 },
    /* CRC-4/INTERLAKEN */
    { { 4, 0x3, 0xF, false, false, 0xF }, 0xB },
    /* CRC-5/EPC-C1G2 */
    { { 5, 0x09, 0x09, false, false, 0x00 }, 0x00 },
    /* CRC-5/G-704 */
    { { 5, 0x15, 0x00, true, true, 0x00 }, 0x07 },
    /* CRC-5/USB */
    { { 5, 0x05, 0x1F, true, true, 0x1F }, 0x19 },
    /* CRC-6/CDMA2000-A */
    { { 6, 0x27, 0x3F, false, false, 0x00 }, 0x0D },
    /* CRC-6/CDMA2000-B */
    { { 6, 0x07, 0x3F, false, false, 0x00 }, 0x3B },
    /* CRC-6/DARC */
    { { 6, 0x19, 0x00, true, true, 0x00 }, 0x26 },
    /* CRC-6/G-704 */
    { { 6, 0x03, 0x00, true, true, 0x00 }, 0x06 },
    /* CRC-6/GSM */
    { { 6, 0x2F, 0x00, false, false, 0x3F }, 0x13 },
    /* CRC-7/MMC */
    { { 7, 0x09, 0x00, false, false, 0x00 }, 0x75 },
    /* CRC-7/ROHC */
    { { 7, 0x4F, 0x7F, true, true, 0x00 }, 0x53 },
    /* CRC-7/UMTS */
    { { 7, 0x45, 0x00, false, false, 0x00 }, 0x61 },
};

TEST(test_catalogue_vectors_below_width_8)
{
    size_t n = sizeof(narrow_vectors) / sizeof(narrow_vectors[0]);
    for (size_t i = 0; i < n; i++) {
        uint32_t crc = 0xFFFFFFFFu;
        ASSERT_EQ(crc_any_compute(&narrow_vectors[i].cfg, check_data, check_len,
                                  &crc), CRC_ANY_OK);
        ASSERT_EQ_HEX(crc, narrow_vectors[i].check);
    }
}

TEST(test_narrow_width_streaming_matches_the_catalogue)
{
    /* CRC-5/USB fed one byte at a time, then finished by hand:
       reflect the 5-bit register, then XOR 0x1F. */
    const crc_any_config_t cfg = { 5, 0x05, 0x1F, true, true, 0x1F };
    uint32_t reg = cfg.init;
    for (size_t i = 0; i < check_len; i++)
        reg = crc_any_update(&cfg, reg, &check_data[i], 1);

    uint32_t reflected = 0;
    for (uint32_t bit = 0; bit < 5u; bit++) {
        if (reg & ((uint32_t)1 << bit))
            reflected |= (uint32_t)1 << (4u - bit);
    }
    ASSERT_EQ_HEX(reflected ^ 0x1Fu, 0x19);
}

TEST(test_width_1_is_the_parity_of_the_message)
{
    /* x + 1 divides a message with an even number of set bits: width 1,
       poly 1 is parity. "123456789" has 3+3+4+3+4+4+5+3+4 = 33 set bits. */
    const crc_any_config_t cfg = { 1, 0x1, 0x0, false, false, 0x0 };
    const uint8_t even[2] = { 0x81, 0x03 };
    uint32_t crc = 0xFFFFFFFFu;

    ASSERT_EQ(crc_any_compute(&cfg, check_data, check_len, &crc), CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 1);
    ASSERT_EQ(crc_any_compute(&cfg, even, sizeof(even), &crc), CRC_ANY_OK);
    ASSERT_EQ_HEX(crc, 0);
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
    RUN(test_update_leaves_the_crc_alone_on_a_bad_width);
    RUN(test_catalogue_vectors_below_width_8);
    RUN(test_narrow_width_streaming_matches_the_catalogue);
    RUN(test_width_1_is_the_parity_of_the_message);
TEST_MAIN_END()
