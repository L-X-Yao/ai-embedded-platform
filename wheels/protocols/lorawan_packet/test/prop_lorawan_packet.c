// SPDX-License-Identifier: MIT
/*
 * prop_lorawan_packet.c — property-based tests for lorawan_packet
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_lorawan_packet.c ../lorawan_packet.c -o prop_lorawan_packet
 */

#include "test_harness.h"
#include "property_test.h"
#include "../lorawan_packet.h"
#include <string.h>

/* Property: data frame build then parse roundtrip */
PROPERTY(data_roundtrip, 10000) {
    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.mtype = (ARBI_UINT8(seed) & 1) ? LORAWAN_UNCONFIRMED_UP : LORAWAN_CONFIRMED_UP;
    frame.dev_addr = ARBI_UINT32(seed);
    frame.fcnt = ARBI_UINT32(seed) & 0xFFFFu; /* 16-bit on wire */
    frame.fport = (uint8_t)ARBI_RANGE(seed, 1, 223);
    frame.ack = (bool)(ARBI_UINT8(seed) & 1);
    frame.adr = (bool)(ARBI_UINT8(seed) & 1);

    uint8_t payload[64];
    uint8_t plen = (uint8_t)ARBI_RANGE(seed, 0, 64);
    ARBI_FILL(seed, payload, plen);
    frame.payload = payload;
    frame.payload_len = plen;
    ARBI_FILL(seed, frame.mic, 4);

    uint8_t buf[256];
    int built = lorawan_build_data(buf, sizeof(buf), &frame);
    PROP_ASSERT(built > 0);

    lorawan_frame_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    lorawan_err_t err = lorawan_parse_data(buf, (uint16_t)built, &parsed);
    PROP_ASSERT_EQ(err, LORAWAN_OK);
    PROP_ASSERT_EQ(parsed.dev_addr, frame.dev_addr);
    PROP_ASSERT_EQ(parsed.payload_len, plen);
    if (plen > 0 && parsed.payload != NULL) {
        PROP_ASSERT_MEM_EQ(parsed.payload, payload, plen);
    }
}

/* Property: join request build is deterministic */
PROPERTY(join_request_determinism, 5000) {
    uint8_t app_eui[8], dev_eui[8], dev_nonce[2];
    ARBI_FILL(seed, app_eui, 8);
    ARBI_FILL(seed, dev_eui, 8);
    ARBI_FILL(seed, dev_nonce, 2);

    uint8_t buf1[32], buf2[32];
    int len1 = lorawan_build_join_request(buf1, sizeof(buf1), app_eui, dev_eui, dev_nonce);
    int len2 = lorawan_build_join_request(buf2, sizeof(buf2), app_eui, dev_eui, dev_nonce);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[256];
    size_t len = ARBI_SIZE(seed, 256);
    ARBI_FILL(seed, garbage, len);

    lorawan_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    lorawan_parse_data(garbage, (uint16_t)len, &frame);

    uint8_t ae[8], de[8], dn[2];
    lorawan_parse_join_request(garbage, (uint16_t)len, ae, de, dn);

    lorawan_get_mtype(len > 0 ? garbage[0] : 0);
}

TEST_MAIN("lorawan_packet properties") {
    RUN(data_roundtrip);
    RUN(join_request_determinism);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
