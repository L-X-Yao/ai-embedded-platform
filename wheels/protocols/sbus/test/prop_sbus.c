/*
 * prop_sbus.c -- property-based tests for sbus
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness
 *        prop_sbus.c ../sbus.c -o prop_sbus
 */

#include "test_harness.h"
#include "property_test.h"
#include "../sbus.h"
#include <string.h>

/* --- Property 1: encode/decode roundtrip preserves all fields --- */

PROPERTY(encode_decode_roundtrip, 10000) {
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));

    int i;
    for (i = 0; i < SBUS_NUM_CHANNELS; i++) {
        f.channels[i] = ARBI_UINT16(seed) & 0x07FF;  /* 11-bit */
    }
    f.ch17       = (ARBI_UINT8(seed) & 1) != 0;
    f.ch18       = (ARBI_UINT8(seed) & 1) != 0;
    f.frame_lost = (ARBI_UINT8(seed) & 1) != 0;
    f.failsafe   = (ARBI_UINT8(seed) & 1) != 0;

    uint8_t buf[SBUS_FRAME_LEN];
    sbus_err_t rc = sbus_encode(&f, buf);
    PROP_ASSERT_EQ(rc, SBUS_OK);

    sbus_frame_t out;
    rc = sbus_decode(buf, &out);
    PROP_ASSERT_EQ(rc, SBUS_OK);

    for (i = 0; i < SBUS_NUM_CHANNELS; i++) {
        PROP_ASSERT_EQ(out.channels[i], f.channels[i]);
    }
    PROP_ASSERT_EQ(out.ch17, f.ch17);
    PROP_ASSERT_EQ(out.ch18, f.ch18);
    PROP_ASSERT_EQ(out.frame_lost, f.frame_lost);
    PROP_ASSERT_EQ(out.failsafe, f.failsafe);
}

/* --- Property 2: encoded frame always has correct header/footer --- */

PROPERTY(header_footer_always_correct, 10000) {
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));

    int i;
    for (i = 0; i < SBUS_NUM_CHANNELS; i++) {
        f.channels[i] = ARBI_UINT16(seed) & 0x07FF;
    }

    uint8_t buf[SBUS_FRAME_LEN];
    sbus_encode(&f, buf);

    PROP_ASSERT_EQ(buf[0], SBUS_HEADER);
    PROP_ASSERT_EQ(buf[24], SBUS_FOOTER);
}

/* --- Property 3: channel values > 2047 are masked to 11 bits --- */

PROPERTY(channel_masking, 10000) {
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));

    int i;
    for (i = 0; i < SBUS_NUM_CHANNELS; i++) {
        f.channels[i] = ARBI_UINT16(seed);  /* may exceed 2047 */
    }

    uint8_t buf[SBUS_FRAME_LEN];
    sbus_encode(&f, buf);

    sbus_frame_t out;
    sbus_decode(buf, &out);

    for (i = 0; i < SBUS_NUM_CHANNELS; i++) {
        PROP_ASSERT(out.channels[i] <= SBUS_CHANNEL_MAX);
        PROP_ASSERT_EQ(out.channels[i], f.channels[i] & 0x07FF);
    }
}

/* --- Property 4: channel_to_us output is in [1000, 2000] --- */

PROPERTY(channel_to_us_range, 10000) {
    uint16_t ch = ARBI_UINT16(seed) & 0x07FF;  /* 11-bit */
    uint16_t us = sbus_channel_to_us(ch);
    PROP_ASSERT(us >= SBUS_US_MIN);
    PROP_ASSERT(us <= SBUS_US_MAX);
}

/* --- Property 5: parser never crashes on random byte streams --- */

PROPERTY(parser_no_crash_random, 10000) {
    sbus_parser_t p;
    sbus_parser_init(&p);

    int n = (int)ARBI_SIZE(seed, 99) + 1;
    int i;
    for (i = 0; i < n; i++) {
        uint8_t byte = ARBI_UINT8(seed);
        sbus_parse_byte(&p, byte);
    }
    /* reaching here without crash is success */
    PROP_ASSERT(p.state >= SBUS_PARSE_SEEKING && p.state <= SBUS_PARSE_ERROR);
}

TEST_MAIN("sbus properties") {
    RUN(encode_decode_roundtrip);
    RUN(header_footer_always_correct);
    RUN(channel_masking);
    RUN(channel_to_us_range);
    RUN(parser_no_crash_random);
}
TEST_MAIN_END()
