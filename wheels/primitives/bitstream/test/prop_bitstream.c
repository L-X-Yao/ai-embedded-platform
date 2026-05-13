// SPDX-License-Identifier: MIT
/*
 * prop_bitstream.c — property-based tests for bitstream
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_bitstream.c ../bitstream.c -o prop_bitstream
 */

#include "test_harness.h"
#include "property_test.h"
#include "../bitstream.h"
#include <string.h>

#define BUF_BYTES 32   /* 256 bits of working space */

/* Property: write N bits then read N bits returns the same value
 * (across N from 1 to 32). */
PROPERTY(write_read_roundtrip, 10000) {
    uint8_t buf[BUF_BYTES];
    memset(buf, 0, sizeof(buf));
    bitstream_t bs;
    PROP_ASSERT_EQ(bitstream_init(&bs, buf, sizeof(buf)), BITSTREAM_OK);

    uint8_t nbits = (uint8_t)ARBI_RANGE(seed, 1, 32);
    uint32_t mask = (nbits >= 32) ? UINT32_MAX
                                  : ((uint32_t)1u << nbits) - 1u;
    uint32_t value = ARBI_UINT32(seed) & mask;

    PROP_ASSERT_EQ(bitstream_write(&bs, value, nbits), BITSTREAM_OK);
    PROP_ASSERT_EQ(bitstream_seek(&bs, 0), BITSTREAM_OK);

    uint32_t out = 0;
    PROP_ASSERT_EQ(bitstream_read(&bs, nbits, &out), BITSTREAM_OK);
    PROP_ASSERT_EQ(out, value);
}

/* Property: sequence of writes followed by sequence of reads recovers
 * the same values in order. */
PROPERTY(sequential_roundtrip, 5000) {
    uint8_t buf[BUF_BYTES];
    memset(buf, 0, sizeof(buf));
    bitstream_t bs;
    bitstream_init(&bs, buf, sizeof(buf));

    uint32_t written_values[10];
    uint8_t  written_bits[10];
    int n = ARBI_RANGE(seed, 1, 10);

    size_t total = 0;
    int actual_n = 0;
    for (int i = 0; i < n; i++) {
        uint8_t nbits = (uint8_t)ARBI_RANGE(seed, 1, 16);
        if (total + nbits > sizeof(buf) * 8) break;
        uint32_t mask = ((uint32_t)1u << nbits) - 1u;
        uint32_t value = ARBI_UINT32(seed) & mask;

        PROP_ASSERT_EQ(bitstream_write(&bs, value, nbits), BITSTREAM_OK);
        written_values[actual_n] = value;
        written_bits[actual_n] = nbits;
        total += nbits;
        actual_n++;
    }

    PROP_ASSERT_EQ(bitstream_tell(&bs), total);

    bitstream_seek(&bs, 0);
    for (int i = 0; i < actual_n; i++) {
        uint32_t out = 0;
        PROP_ASSERT_EQ(
            bitstream_read(&bs, written_bits[i], &out),
            BITSTREAM_OK);
        PROP_ASSERT_EQ(out, written_values[i]);
    }
}

/* Property: tell() advances by nbits after each write/read */
PROPERTY(tell_advances_correctly, 10000) {
    uint8_t buf[BUF_BYTES];
    memset(buf, 0, sizeof(buf));
    bitstream_t bs;
    bitstream_init(&bs, buf, sizeof(buf));

    uint8_t nbits = (uint8_t)ARBI_RANGE(seed, 1, 32);
    uint32_t mask = (nbits >= 32) ? UINT32_MAX
                                  : ((uint32_t)1u << nbits) - 1u;
    uint32_t value = ARBI_UINT32(seed) & mask;

    size_t before = bitstream_tell(&bs);
    PROP_ASSERT_EQ(bitstream_write(&bs, value, nbits), BITSTREAM_OK);
    size_t after = bitstream_tell(&bs);
    PROP_ASSERT_EQ(after - before, (size_t)nbits);
}

/* Property: read of just-written value returns masked input regardless
 * of garbage high bits in the source value (LSB-first packing). */
PROPERTY(high_bits_ignored, 10000) {
    uint8_t buf[BUF_BYTES];
    memset(buf, 0, sizeof(buf));
    bitstream_t bs;
    bitstream_init(&bs, buf, sizeof(buf));

    uint8_t nbits = (uint8_t)ARBI_RANGE(seed, 1, 31);
    uint32_t input = ARBI_UINT32(seed);
    uint32_t mask = ((uint32_t)1u << nbits) - 1u;

    PROP_ASSERT_EQ(bitstream_write(&bs, input, nbits), BITSTREAM_OK);
    bitstream_seek(&bs, 0);
    uint32_t out = 0;
    PROP_ASSERT_EQ(bitstream_read(&bs, nbits, &out), BITSTREAM_OK);
    PROP_ASSERT_EQ(out, input & mask);
}

/* Property: reset returns position to 0 */
PROPERTY(reset_zeroes_position, 5000) {
    uint8_t buf[BUF_BYTES];
    memset(buf, 0, sizeof(buf));
    bitstream_t bs;
    bitstream_init(&bs, buf, sizeof(buf));

    /* fill in a few values */
    uint8_t nbits = (uint8_t)ARBI_RANGE(seed, 1, 8);
    uint32_t value = ARBI_UINT32(seed) & (((uint32_t)1u << nbits) - 1u);
    bitstream_write(&bs, value, nbits);
    PROP_ASSERT(bitstream_tell(&bs) > 0);

    PROP_ASSERT_EQ(bitstream_reset(&bs), BITSTREAM_OK);
    PROP_ASSERT_EQ(bitstream_tell(&bs), 0u);
}

/* Property: write beyond capacity returns ERR_OVERFLOW */
PROPERTY(overflow_rejected, 5000) {
    uint8_t buf[1];   /* only 8 bits available */
    bitstream_t bs;
    bitstream_init(&bs, buf, sizeof(buf));

    /* Use seed-derived value so the property does not become trivial
     * across iterations and to satisfy -Wunused-parameter. */
    uint32_t value = ARBI_UINT32(seed) & 0xFFu;
    PROP_ASSERT_EQ(bitstream_write(&bs, value, 8), BITSTREAM_OK);
    PROP_ASSERT_EQ(bitstream_write(&bs, 1, 1), BITSTREAM_ERR_OVERFLOW);
}

/* Property: writing arbitrary bytes via the API does not crash */
PROPERTY(stress_no_crash, 5000) {
    uint8_t buf[BUF_BYTES];
    ARBI_FILL(seed, buf, sizeof(buf));
    bitstream_t bs;
    bitstream_init(&bs, buf, sizeof(buf));
    /* random read sequence */
    int n = ARBI_RANGE(seed, 1, 20);
    for (int i = 0; i < n; i++) {
        uint8_t nbits = (uint8_t)ARBI_RANGE(seed, 1, 32);
        uint32_t out = 0;
        (void)bitstream_read(&bs, nbits, &out);
    }
    PROP_ASSERT(1);
}

TEST_MAIN("bitstream properties") {
    RUN(write_read_roundtrip);
    RUN(sequential_roundtrip);
    RUN(tell_advances_correctly);
    RUN(high_bits_ignored);
    RUN(reset_zeroes_position);
    RUN(overflow_rejected);
    RUN(stress_no_crash);
}
TEST_MAIN_END()
