#include "test_harness.h"
#include <string.h>
#include "endian.h"

/* ── Byte swap ─────────────────────────────────────────────────── */

TEST(test_swap16)
{
    ASSERT_EQ(endian_swap16(0x0102u), 0x0201u);
    ASSERT_EQ(endian_swap16(0xAABBu), 0xBBAAu);
}

TEST(test_swap32)
{
    ASSERT_EQ(endian_swap32(0x01020304u), 0x04030201u);
    ASSERT_EQ(endian_swap32(0xDEADBEEFu), 0xEFBEADDEu);
}

TEST(test_swap64)
{
    ASSERT_EQ(endian_swap64(0x0102030405060708ull),
                             0x0807060504030201ull);
}

/* ── Zero values ───────────────────────────────────────────────── */

TEST(test_swap_zero)
{
    ASSERT_EQ(endian_swap16(0), 0u);
    ASSERT_EQ(endian_swap32(0), 0u);
    ASSERT_EQ(endian_swap64(0), 0ull);
}

/* ── Max values ────────────────────────────────────────────────── */

TEST(test_swap_max)
{
    ASSERT_EQ(endian_swap16(0xFFFFu), 0xFFFFu);
    ASSERT_EQ(endian_swap32(0xFFFFFFFFu), 0xFFFFFFFFu);
    ASSERT_EQ(endian_swap64(0xFFFFFFFFFFFFFFFFull), 0xFFFFFFFFFFFFFFFFull);
}

/* ── Double swap is identity ───────────────────────────────────── */

TEST(test_swap_roundtrip)
{
    ASSERT_EQ(endian_swap16(endian_swap16(0x1234u)), 0x1234u);
    ASSERT_EQ(endian_swap32(endian_swap32(0x12345678u)), 0x12345678u);
    ASSERT_EQ(endian_swap64(endian_swap64(0x123456789ABCDEF0ull)),
                                          0x123456789ABCDEF0ull);
}

/* ── Runtime detection ─────────────────────────────────────────── */

TEST(test_is_little)
{
    /* Verify with independent probe */
    const uint16_t probe = 1u;
    bool expected = (*(const uint8_t *)&probe) == 1u;
    ASSERT_EQ(endian_is_little(), expected);
}

/* ── htobe / betoh roundtrip ───────────────────────────────────── */

TEST(test_htobe_betoh_roundtrip16)
{
    uint16_t orig = 0xCAFEu;
    ASSERT_EQ(endian_be16toh(endian_htobe16(orig)), orig);
}

TEST(test_htobe_betoh_roundtrip32)
{
    uint32_t orig = 0xDEADBEEFu;
    ASSERT_EQ(endian_be32toh(endian_htobe32(orig)), orig);
}

TEST(test_htobe_betoh_roundtrip64)
{
    uint64_t orig = 0x0102030405060708ull;
    ASSERT_EQ(endian_be64toh(endian_htobe64(orig)), orig);
}

/* ── htole / letoh roundtrip ───────────────────────────────────── */

TEST(test_htole_letoh_roundtrip16)
{
    uint16_t orig = 0xBEEFu;
    ASSERT_EQ(endian_le16toh(endian_htole16(orig)), orig);
}

TEST(test_htole_letoh_roundtrip32)
{
    uint32_t orig = 0x12345678u;
    ASSERT_EQ(endian_le32toh(endian_htole32(orig)), orig);
}

TEST(test_htole_letoh_roundtrip64)
{
    uint64_t orig = 0xFEDCBA9876543210ull;
    ASSERT_EQ(endian_le64toh(endian_htole64(orig)), orig);
}

/* ── Identity on native endianness ─────────────────────────────── */

TEST(test_native_identity)
{
    uint32_t val = 0x11223344u;
    if (endian_is_little()) {
        ASSERT_EQ(endian_htole32(val), val);
        ASSERT_EQ(endian_le32toh(val), val);
    } else {
        ASSERT_EQ(endian_htobe32(val), val);
        ASSERT_EQ(endian_be32toh(val), val);
    }
}

/* ── Buffer read/write big-endian ──────────────────────────────── */

TEST(test_read_write_be16)
{
    uint8_t buf[2];
    endian_write_be16(buf, 0x0102u);
    ASSERT_EQ(buf[0], 0x01u);
    ASSERT_EQ(buf[1], 0x02u);
    ASSERT_EQ(endian_read_be16(buf), 0x0102u);
}

TEST(test_read_write_be32)
{
    uint8_t buf[4];
    endian_write_be32(buf, 0x01020304u);
    ASSERT_EQ(buf[0], 0x01u);
    ASSERT_EQ(buf[1], 0x02u);
    ASSERT_EQ(buf[2], 0x03u);
    ASSERT_EQ(buf[3], 0x04u);
    ASSERT_EQ(endian_read_be32(buf), 0x01020304u);
}

/* ── Buffer read/write little-endian ───────────────────────────── */

TEST(test_read_write_le16)
{
    uint8_t buf[2];
    endian_write_le16(buf, 0x0102u);
    ASSERT_EQ(buf[0], 0x02u);
    ASSERT_EQ(buf[1], 0x01u);
    ASSERT_EQ(endian_read_le16(buf), 0x0102u);
}

TEST(test_read_write_le32)
{
    uint8_t buf[4];
    endian_write_le32(buf, 0x01020304u);
    ASSERT_EQ(buf[0], 0x04u);
    ASSERT_EQ(buf[1], 0x03u);
    ASSERT_EQ(buf[2], 0x02u);
    ASSERT_EQ(buf[3], 0x01u);
    ASSERT_EQ(endian_read_le32(buf), 0x01020304u);
}

/* ── Buffer alignment independence ─────────────────────────────── */

TEST(test_unaligned_buffer)
{
    /* Read/write at offset 1 to force unaligned access */
    uint8_t raw[8];
    memset(raw, 0, sizeof(raw));

    endian_write_be32(raw + 1, 0xAABBCCDDu);
    ASSERT_EQ(endian_read_be32(raw + 1), 0xAABBCCDDu);

    endian_write_le32(raw + 1, 0x11223344u);
    ASSERT_EQ(endian_read_le32(raw + 1), 0x11223344u);
}

/* ── Known byte pattern verification ───────────────────────────── */

TEST(test_known_byte_pattern)
{
    /* 0x0102 in big-endian wire order is [01, 02] */
    const uint8_t be_wire[] = { 0x01, 0x02 };
    ASSERT_EQ(endian_read_be16(be_wire), 0x0102u);

    /* 0x0102 in little-endian wire order is [02, 01] */
    const uint8_t le_wire[] = { 0x02, 0x01 };
    ASSERT_EQ(endian_read_le16(le_wire), 0x0102u);
}

/* ── htobe produces correct wire bytes ─────────────────────────── */

TEST(test_htobe_wire_bytes)
{
    /* Regardless of host endianness, htobe32 of 0x01020304
     * when stored and read as big-endian should give back 0x01020304 */
    uint32_t be_val = endian_htobe32(0x01020304u);
    uint8_t buf[4];
    memcpy(buf, &be_val, 4);
    ASSERT_EQ(endian_read_be32(buf), 0x01020304u);
}

TEST_MAIN("endian")
    RUN(test_swap16);
    RUN(test_swap32);
    RUN(test_swap64);
    RUN(test_swap_zero);
    RUN(test_swap_max);
    RUN(test_swap_roundtrip);
    RUN(test_is_little);
    RUN(test_htobe_betoh_roundtrip16);
    RUN(test_htobe_betoh_roundtrip32);
    RUN(test_htobe_betoh_roundtrip64);
    RUN(test_htole_letoh_roundtrip16);
    RUN(test_htole_letoh_roundtrip32);
    RUN(test_htole_letoh_roundtrip64);
    RUN(test_native_identity);
    RUN(test_read_write_be16);
    RUN(test_read_write_be32);
    RUN(test_read_write_le16);
    RUN(test_read_write_le32);
    RUN(test_unaligned_buffer);
    RUN(test_known_byte_pattern);
    RUN(test_htobe_wire_bytes);
TEST_MAIN_END()
