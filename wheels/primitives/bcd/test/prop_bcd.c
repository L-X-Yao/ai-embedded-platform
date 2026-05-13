// SPDX-License-Identifier: MIT
/*
 * prop_bcd.c — property-based tests for bcd
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_bcd.c ../bcd.c -o prop_bcd
 */

#include "test_harness.h"
#include "property_test.h"
#include "../bcd.h"
#include <string.h>

/* Property: bcd_from_bin then bcd_to_bin returns original integer */
PROPERTY(scalar_roundtrip, 10000) {
    uint8_t bin = (uint8_t)ARBI_RANGE(seed, 0, 99);
    uint8_t encoded = bcd_from_bin(bin);
    uint8_t decoded = bcd_to_bin(encoded);
    PROP_ASSERT_EQ(decoded, bin);
}

/* Property: validating arbitrary bytes never crashes and matches the
 * formal definition (both nibbles must be <= 9). */
PROPERTY(validate_arbitrary_no_crash, 10000) {
    uint8_t bytes[16];
    size_t len = ARBI_SIZE(seed, 16);
    ARBI_FILL(seed, bytes, len);
    for (size_t i = 0; i < len; i++) {
        bool ok = bcd_is_valid(bytes[i]);
        bool expected = ((bytes[i] >> 4) <= 9) && ((bytes[i] & 0x0F) <= 9);
        PROP_ASSERT_EQ(ok, expected);
    }
}

/* Property: bcd_from_bin output is always a valid BCD byte (both nibbles 0-9) */
PROPERTY(from_bin_produces_valid, 10000) {
    uint8_t bin = (uint8_t)ARBI_RANGE(seed, 0, 99);
    uint8_t encoded = bcd_from_bin(bin);
    PROP_ASSERT(bcd_is_valid(encoded));
    PROP_ASSERT(((encoded >> 4) & 0x0F) <= 9);
    PROP_ASSERT((encoded & 0x0F) <= 9);
}

/* Property: bcd_buf_from_bin then bcd_buf_to_bin returns original buffer */
PROPERTY(buffer_roundtrip, 5000) {
    uint8_t bin_in[64];
    uint8_t bcd_buf[64];
    uint8_t bin_out[64];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 64);
    for (uint16_t i = 0; i < len; i++) {
        bin_in[i] = (uint8_t)ARBI_RANGE(seed, 0, 99);
    }

    PROP_ASSERT_EQ(bcd_buf_from_bin(bin_in, bcd_buf, len), BCD_OK);
    PROP_ASSERT_EQ(bcd_buf_to_bin(bcd_buf, bin_out, len), BCD_OK);
    PROP_ASSERT_MEM_EQ(bin_out, bin_in, len);
}

/* Property: safe variant agrees with unsafe variant on valid input */
PROPERTY(safe_matches_unsafe, 10000) {
    uint8_t bin = (uint8_t)ARBI_RANGE(seed, 0, 99);
    uint8_t safe_out = 0;
    PROP_ASSERT_EQ(bcd_from_bin_safe(bin, &safe_out), BCD_OK);
    PROP_ASSERT_EQ(safe_out, bcd_from_bin(bin));

    uint8_t encoded = bcd_from_bin(bin);
    uint8_t safe_bin = 0;
    PROP_ASSERT_EQ(bcd_to_bin_safe(encoded, &safe_bin), BCD_OK);
    PROP_ASSERT_EQ(safe_bin, bcd_to_bin(encoded));
}

/* Property: out-of-range bin (>99) rejected by safe variant */
PROPERTY(out_of_range_rejected, 10000) {
    uint8_t bin = (uint8_t)ARBI_RANGE(seed, 100, 255);
    uint8_t out = 0;
    PROP_ASSERT_EQ(bcd_from_bin_safe(bin, &out), BCD_ERR_RANGE);
}

TEST_MAIN("bcd properties") {
    RUN(scalar_roundtrip);
    RUN(validate_arbitrary_no_crash);
    RUN(from_bin_produces_valid);
    RUN(buffer_roundtrip);
    RUN(safe_matches_unsafe);
    RUN(out_of_range_rejected);
}
TEST_MAIN_END()
