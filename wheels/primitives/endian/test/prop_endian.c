/*
 * prop_endian.c — property-based tests for endian
 *
 * Requires: property_test.h from tooling/test/
 * Build:
 *   gcc -std=c99 -I.. -I../../../../tooling/test prop_endian.c ../endian.c -o prop_endian
 */

#include "test_harness.h"
#include "property_test.h"
#include "../endian.h"
#include <string.h>

/* Property: swap16 is its own inverse */
PROPERTY(swap16_involution, 50000) {
    uint16_t val = ARBI_UINT16(seed);
    PROP_ASSERT_EQ(endian_swap16(endian_swap16(val)), val);
}

/* Property: swap32 is its own inverse */
PROPERTY(swap32_involution, 50000) {
    uint32_t val = ARBI_UINT32(seed);
    PROP_ASSERT_EQ(endian_swap32(endian_swap32(val)), val);
}

/* Property: htobe/betoh roundtrip for 16-bit */
PROPERTY(htobe16_roundtrip, 10000) {
    uint16_t val = ARBI_UINT16(seed);
    PROP_ASSERT_EQ(endian_be16toh(endian_htobe16(val)), val);
}

/* Property: htobe/betoh roundtrip for 32-bit */
PROPERTY(htobe32_roundtrip, 10000) {
    uint32_t val = ARBI_UINT32(seed);
    PROP_ASSERT_EQ(endian_be32toh(endian_htobe32(val)), val);
}

/* Property: htole/letoh roundtrip for 16-bit */
PROPERTY(htole16_roundtrip, 10000) {
    uint16_t val = ARBI_UINT16(seed);
    PROP_ASSERT_EQ(endian_le16toh(endian_htole16(val)), val);
}

/* Property: htole/letoh roundtrip for 32-bit */
PROPERTY(htole32_roundtrip, 10000) {
    uint32_t val = ARBI_UINT32(seed);
    PROP_ASSERT_EQ(endian_le32toh(endian_htole32(val)), val);
}

/* Property: write_be/read_be roundtrip for 16/32-bit */
PROPERTY(buf_be_roundtrip, 10000) {
    uint8_t buf16[2], buf32[4];

    uint16_t v16 = ARBI_UINT16(seed);
    endian_write_be16(buf16, v16);
    PROP_ASSERT_EQ(endian_read_be16(buf16), v16);

    uint32_t v32 = ARBI_UINT32(seed);
    endian_write_be32(buf32, v32);
    PROP_ASSERT_EQ(endian_read_be32(buf32), v32);
}

/* Property: write_le/read_le roundtrip for 16/32-bit */
PROPERTY(buf_le_roundtrip, 10000) {
    uint8_t buf16[2], buf32[4];

    uint16_t v16 = ARBI_UINT16(seed);
    endian_write_le16(buf16, v16);
    PROP_ASSERT_EQ(endian_read_le16(buf16), v16);

    uint32_t v32 = ARBI_UINT32(seed);
    endian_write_le32(buf32, v32);
    PROP_ASSERT_EQ(endian_read_le32(buf32), v32);
}

/* Property: BE and LE buffer representations differ for values > 0xFF */
PROPERTY(be_le_differ, 10000) {
    uint16_t val = ARBI_UINT16(seed);
    if (val <= 0xFF) return; /* single byte — could be the same */

    uint8_t be_buf[2], le_buf[2];
    endian_write_be16(be_buf, val);
    endian_write_le16(le_buf, val);

    /* If high and low bytes differ, BE and LE must differ */
    if ((val >> 8) != (val & 0xFF)) {
        PROP_ASSERT(memcmp(be_buf, le_buf, 2) != 0);
    }
}

TEST_MAIN("endian properties") {
    RUN(swap16_involution);
    RUN(swap32_involution);
    RUN(htobe16_roundtrip);
    RUN(htobe32_roundtrip);
    RUN(htole16_roundtrip);
    RUN(htole32_roundtrip);
    RUN(buf_be_roundtrip);
    RUN(buf_le_roundtrip);
    RUN(be_le_differ);
}
TEST_MAIN_END()
