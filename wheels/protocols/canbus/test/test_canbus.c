// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "canbus.h"
#include <string.h>

/* ---------- validate ---------- */

TEST(test_validate_standard_ok)
{
    canbus_frame_t f = { .id = 0x123, .dlc = 8, .extended = false, .rtr = false };
    ASSERT_EQ(canbus_frame_validate(&f), CANBUS_OK);
}

TEST(test_validate_extended_ok)
{
    canbus_frame_t f = { .id = 0x1FEDCBA, .dlc = 4, .extended = true, .rtr = false };
    ASSERT_EQ(canbus_frame_validate(&f), CANBUS_OK);
}

TEST(test_validate_rtr_ok)
{
    canbus_frame_t f = { .id = 0x100, .dlc = 8, .extended = false, .rtr = true };
    ASSERT_EQ(canbus_frame_validate(&f), CANBUS_OK);
}

TEST(test_validate_null)
{
    ASSERT_EQ(canbus_frame_validate(NULL), CANBUS_ERR_PARAM);
}

TEST(test_validate_standard_id_too_large)
{
    canbus_frame_t f = { .id = 0x800, .dlc = 0, .extended = false };
    ASSERT_EQ(canbus_frame_validate(&f), CANBUS_ERR_ID);
}

TEST(test_validate_extended_id_too_large)
{
    canbus_frame_t f = { .id = 0x20000000u, .dlc = 0, .extended = true };
    ASSERT_EQ(canbus_frame_validate(&f), CANBUS_ERR_ID);
}

TEST(test_validate_dlc_too_large)
{
    canbus_frame_t f = { .id = 0x100, .dlc = 9, .extended = false };
    ASSERT_EQ(canbus_frame_validate(&f), CANBUS_ERR_DLC);
}

/* ---------- pack/unpack roundtrip ---------- */

TEST(test_pack_unpack_standard)
{
    canbus_frame_t tx = {
        .id = 0x7AB, .dlc = 3, .extended = false, .rtr = false,
        .data = {0xDE, 0xAD, 0xBE}
    };
    uint8_t buf[16];
    int n = canbus_frame_pack(&tx, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(n, 3 + 3); /* 2 ID + 1 flags + 3 data */

    canbus_frame_t rx;
    ASSERT_EQ(canbus_frame_unpack(buf, (uint16_t)n, &rx), CANBUS_OK);
    ASSERT_EQ(rx.id, 0x7AB);
    ASSERT_EQ(rx.dlc, 3);
    ASSERT_FALSE(rx.extended);
    ASSERT_FALSE(rx.rtr);
    ASSERT_EQ(rx.data[0], 0xDE);
    ASSERT_EQ(rx.data[1], 0xAD);
    ASSERT_EQ(rx.data[2], 0xBE);
}

TEST(test_pack_unpack_extended)
{
    canbus_frame_t tx = {
        .id = 0x1ABCDEF0, .dlc = 8, .extended = true, .rtr = false,
        .data = {1, 2, 3, 4, 5, 6, 7, 8}
    };
    uint8_t buf[16];
    int n = canbus_frame_pack(&tx, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(n, 5 + 8); /* 4 ID + 1 flags + 8 data */

    canbus_frame_t rx;
    ASSERT_EQ(canbus_frame_unpack(buf, (uint16_t)n, &rx), CANBUS_OK);
    ASSERT_EQ_HEX(rx.id, 0x1ABCDEF0);
    ASSERT_EQ(rx.dlc, 8);
    ASSERT_TRUE(rx.extended);
    ASSERT_FALSE(rx.rtr);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(rx.data[i], (uint8_t)(i + 1));
    }
}

TEST(test_pack_unpack_rtr_no_data)
{
    canbus_frame_t tx = {
        .id = 0x200, .dlc = 4, .extended = false, .rtr = true,
        .data = {0xFF, 0xFF, 0xFF, 0xFF}  /* should be ignored */
    };
    uint8_t buf[16];
    int n = canbus_frame_pack(&tx, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(n, 3); /* 2 ID + 1 flags, no data for RTR */

    canbus_frame_t rx;
    ASSERT_EQ(canbus_frame_unpack(buf, (uint16_t)n, &rx), CANBUS_OK);
    ASSERT_EQ(rx.id, 0x200);
    ASSERT_EQ(rx.dlc, 4);
    ASSERT_TRUE(rx.rtr);
    /* Data should be zeroed out */
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(rx.data[i], 0);
    }
}

TEST(test_pack_unpack_zero_dlc)
{
    canbus_frame_t tx = { .id = 0x001, .dlc = 0, .extended = false, .rtr = false };
    uint8_t buf[16];
    int n = canbus_frame_pack(&tx, buf, sizeof(buf));
    ASSERT_EQ(n, 3); /* 2 ID + 1 flags, 0 data */

    canbus_frame_t rx;
    ASSERT_EQ(canbus_frame_unpack(buf, (uint16_t)n, &rx), CANBUS_OK);
    ASSERT_EQ(rx.id, 0x001);
    ASSERT_EQ(rx.dlc, 0);
}

/* ---------- pack error cases ---------- */

TEST(test_pack_null_params)
{
    canbus_frame_t f = { .id = 0x100, .dlc = 0 };
    uint8_t buf[16];
    ASSERT_EQ(canbus_frame_pack(NULL, buf, sizeof(buf)), CANBUS_ERR_PARAM);
    ASSERT_EQ(canbus_frame_pack(&f, NULL, sizeof(buf)), CANBUS_ERR_PARAM);
}

TEST(test_pack_overflow)
{
    canbus_frame_t f = { .id = 0x100, .dlc = 8, .extended = false };
    uint8_t buf[4]; /* too small for 3 + 8 = 11 bytes */
    ASSERT_EQ(canbus_frame_pack(&f, buf, sizeof(buf)), CANBUS_ERR_OVERFLOW);
}

/* ---------- unpack error cases ---------- */

TEST(test_unpack_null_params)
{
    uint8_t buf[8] = {0};
    canbus_frame_t f;
    ASSERT_EQ(canbus_frame_unpack(NULL, 8, &f), CANBUS_ERR_PARAM);
    ASSERT_EQ(canbus_frame_unpack(buf, 8, NULL), CANBUS_ERR_PARAM);
}

TEST(test_unpack_too_short)
{
    uint8_t buf[2] = {0};
    canbus_frame_t f;
    ASSERT_EQ(canbus_frame_unpack(buf, 2, &f), CANBUS_ERR_SIZE);
}

/* ---------- filter matching ---------- */

TEST(test_filter_exact_match)
{
    canbus_filter_t flt = { .id = 0x100, .mask = 0x7FF, .extended = false };
    canbus_frame_t yes = { .id = 0x100, .extended = false };
    canbus_frame_t no  = { .id = 0x101, .extended = false };
    ASSERT_TRUE(canbus_filter_match(&flt, &yes));
    ASSERT_FALSE(canbus_filter_match(&flt, &no));
}

TEST(test_filter_masked)
{
    /* Match any ID where bits 10:8 = 0x1 (0x100-0x1FF) */
    canbus_filter_t flt = { .id = 0x100, .mask = 0x700, .extended = false };
    canbus_frame_t yes1 = { .id = 0x100, .extended = false };
    canbus_frame_t yes2 = { .id = 0x1FF, .extended = false };
    canbus_frame_t no   = { .id = 0x200, .extended = false };
    ASSERT_TRUE(canbus_filter_match(&flt, &yes1));
    ASSERT_TRUE(canbus_filter_match(&flt, &yes2));
    ASSERT_FALSE(canbus_filter_match(&flt, &no));
}

TEST(test_filter_wildcard)
{
    /* mask=0 → match everything with same extended flag */
    canbus_filter_t flt = { .id = 0, .mask = 0, .extended = false };
    canbus_frame_t any = { .id = 0x7FF, .extended = false };
    ASSERT_TRUE(canbus_filter_match(&flt, &any));
}

TEST(test_filter_extended_mismatch)
{
    canbus_filter_t flt = { .id = 0x100, .mask = 0x7FF, .extended = true };
    canbus_frame_t f = { .id = 0x100, .extended = false };
    ASSERT_FALSE(canbus_filter_match(&flt, &f));
}

TEST(test_filter_null)
{
    canbus_frame_t f = { .id = 0x100 };
    ASSERT_FALSE(canbus_filter_match(NULL, &f));
    canbus_filter_t flt = { .id = 0 };
    ASSERT_FALSE(canbus_filter_match(&flt, NULL));
}

/* ---------- signal extract/pack (little-endian / Intel) ---------- */

TEST(test_signal_extract_8bit_le)
{
    /* 8-bit value at byte 0, bits 0-7 */
    uint8_t data[8] = {0xAB, 0, 0, 0, 0, 0, 0, 0};
    uint64_t val = canbus_signal_extract(data, 0, 8, false);
    ASSERT_EQ_HEX(val, 0xAB);
}

TEST(test_signal_extract_16bit_le)
{
    /* 16-bit value starting at bit 0 (bytes 0-1, Intel order) */
    uint8_t data[8] = {0x34, 0x12, 0, 0, 0, 0, 0, 0};
    uint64_t val = canbus_signal_extract(data, 0, 16, false);
    ASSERT_EQ_HEX(val, 0x1234);
}

TEST(test_signal_extract_cross_byte_le)
{
    /* 12-bit signal starting at bit 4 (nibble-aligned across bytes 0-1) */
    /* data[0] = 0xF0 → bits 4-7 = 0xF (lower 4 bits of signal)
     * data[1] = 0xAB → bits 0-7 → upper 8 bits of signal
     * Signal = 0xABF */
    uint8_t data[8] = {0xF0, 0xAB, 0, 0, 0, 0, 0, 0};
    uint64_t val = canbus_signal_extract(data, 4, 12, false);
    ASSERT_EQ_HEX(val, 0xABF);
}

/* ---------- signal extract/pack (big-endian / Motorola) ---------- */

TEST(test_signal_extract_8bit_be)
{
    /* 8-bit Motorola signal starting at bit 7 (MSB of byte 0) */
    uint8_t data[8] = {0xCD, 0, 0, 0, 0, 0, 0, 0};
    uint64_t val = canbus_signal_extract(data, 7, 8, true);
    ASSERT_EQ_HEX(val, 0xCD);
}

TEST(test_signal_extract_16bit_be)
{
    /* 16-bit Motorola signal: start_bit=7 (MSB at byte0 bit7), 16 bits
     * spans byte0 and byte1 in Motorola order */
    uint8_t data[8] = {0x12, 0x34, 0, 0, 0, 0, 0, 0};
    uint64_t val = canbus_signal_extract(data, 7, 16, true);
    ASSERT_EQ_HEX(val, 0x1234);
}

/* ---------- signal pack ---------- */

TEST(test_signal_pack_le)
{
    uint8_t data[8] = {0};
    canbus_signal_pack(data, 0, 16, false, 0x1234);
    ASSERT_EQ_HEX(data[0], 0x34);
    ASSERT_EQ_HEX(data[1], 0x12);
}

TEST(test_signal_pack_be)
{
    uint8_t data[8] = {0};
    canbus_signal_pack(data, 7, 16, true, 0x1234);
    ASSERT_EQ_HEX(data[0], 0x12);
    ASSERT_EQ_HEX(data[1], 0x34);
}

TEST(test_signal_pack_extract_roundtrip)
{
    uint8_t data[8] = {0};
    uint64_t original = 0xDEAD;
    canbus_signal_pack(data, 8, 16, false, original);
    uint64_t extracted = canbus_signal_extract(data, 8, 16, false);
    ASSERT_EQ_HEX(extracted, original);
}

/* ---------- helpers ---------- */

TEST(test_is_standard_id)
{
    ASSERT_TRUE(canbus_is_standard_id(0));
    ASSERT_TRUE(canbus_is_standard_id(0x7FF));
    ASSERT_FALSE(canbus_is_standard_id(0x800));
}

TEST(test_is_extended_id)
{
    ASSERT_TRUE(canbus_is_extended_id(0));
    ASSERT_TRUE(canbus_is_extended_id(0x1FFFFFFF));
    ASSERT_FALSE(canbus_is_extended_id(0x20000000));
}

/* ---------- signal edge: null data ---------- */

TEST(test_signal_extract_null)
{
    ASSERT_EQ(canbus_signal_extract(NULL, 0, 8, false), 0);
}

TEST(test_signal_extract_zero_length)
{
    uint8_t data[8] = {0xFF};
    ASSERT_EQ(canbus_signal_extract(data, 0, 0, false), 0);
}

TEST_MAIN("canbus")
    RUN(test_validate_standard_ok);
    RUN(test_validate_extended_ok);
    RUN(test_validate_rtr_ok);
    RUN(test_validate_null);
    RUN(test_validate_standard_id_too_large);
    RUN(test_validate_extended_id_too_large);
    RUN(test_validate_dlc_too_large);
    RUN(test_pack_unpack_standard);
    RUN(test_pack_unpack_extended);
    RUN(test_pack_unpack_rtr_no_data);
    RUN(test_pack_unpack_zero_dlc);
    RUN(test_pack_null_params);
    RUN(test_pack_overflow);
    RUN(test_unpack_null_params);
    RUN(test_unpack_too_short);
    RUN(test_filter_exact_match);
    RUN(test_filter_masked);
    RUN(test_filter_wildcard);
    RUN(test_filter_extended_mismatch);
    RUN(test_filter_null);
    RUN(test_signal_extract_8bit_le);
    RUN(test_signal_extract_16bit_le);
    RUN(test_signal_extract_cross_byte_le);
    RUN(test_signal_extract_8bit_be);
    RUN(test_signal_extract_16bit_be);
    RUN(test_signal_pack_le);
    RUN(test_signal_pack_be);
    RUN(test_signal_pack_extract_roundtrip);
    RUN(test_is_standard_id);
    RUN(test_is_extended_id);
    RUN(test_signal_extract_null);
    RUN(test_signal_extract_zero_length);
TEST_MAIN_END()
