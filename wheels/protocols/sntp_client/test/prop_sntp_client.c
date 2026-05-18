// SPDX-License-Identifier: MIT
/*
 * prop_sntp_client.c -- property-based tests for sntp_client
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness
 *        prop_sntp_client.c ../sntp_client.c -o prop_sntp_client
 */

#include "test_harness.h"
#include "property_test.h"
#include "../sntp_client.h"
#include <string.h>

/* helper: build a valid server response packet */
static void build_valid_response(uint8_t *pkt, uint8_t stratum,
                                 uint32_t rx_s, uint32_t tx_s)
{
    memset(pkt, 0, SNTP_PACKET_SIZE);
    /* LI=0, VN=4, Mode=4 (server) */
    pkt[0] = (0 << 6) | (4 << 3) | 4;
    pkt[1] = stratum;
    /* receive timestamp at offset 32 */
    pkt[32] = (uint8_t)(rx_s >> 24); pkt[33] = (uint8_t)(rx_s >> 16);
    pkt[34] = (uint8_t)(rx_s >>  8); pkt[35] = (uint8_t)(rx_s);
    /* transmit timestamp at offset 40 */
    pkt[40] = (uint8_t)(tx_s >> 24); pkt[41] = (uint8_t)(tx_s >> 16);
    pkt[42] = (uint8_t)(tx_s >>  8); pkt[43] = (uint8_t)(tx_s);
}

/* --- Property 1: unix_to_ntp/ntp_to_unix roundtrip --- */

PROPERTY(unix_ntp_roundtrip, 10000) {
    /*
     * unix_to_ntp adds SNTP_UNIX_OFFSET (~2.2 billion) to unix_sec.
     * To avoid uint32_t overflow, cap unix_sec so
     * unix_sec + SNTP_UNIX_OFFSET fits in 32 bits.
     */
    uint32_t max_unix = UINT32_MAX - (uint32_t)SNTP_UNIX_OFFSET;
    uint32_t unix_sec = ARBI_UINT32(seed) % (max_unix + 1);
    uint16_t ms = (uint16_t)(ARBI_UINT16(seed) % 1000);

    sntp_timestamp_t ntp = sntp_client_unix_to_ntp(unix_sec, ms);
    uint32_t out_sec;
    uint16_t out_ms;
    sntp_client_ntp_to_unix(&ntp, &out_sec, &out_ms);

    PROP_ASSERT_EQ(out_sec, unix_sec);
    /* ms may lose sub-ms precision due to fraction rounding */
    int diff = (int)out_ms - (int)ms;
    if (diff < 0) diff = -diff;
    PROP_ASSERT(diff <= 1);
}

/* --- Property 2: build request produces exactly 48 bytes --- */

PROPERTY(request_always_48_bytes, 10000) {
    uint8_t buf[64];
    sntp_timestamp_t tx = {
        .seconds  = ARBI_UINT32(seed),
        .fraction = ARBI_UINT32(seed),
    };

    int rc = sntp_client_build_request(buf, sizeof(buf), &tx);
    PROP_ASSERT_EQ(rc, SNTP_PACKET_SIZE);
}

/* --- Property 3: request always has mode=client(3) and version=4 --- */

PROPERTY(request_mode_and_version, 10000) {
    uint8_t buf[SNTP_PACKET_SIZE];
    sntp_timestamp_t tx = {
        .seconds  = ARBI_UINT32(seed),
        .fraction = ARBI_UINT32(seed),
    };

    sntp_client_build_request(buf, sizeof(buf), &tx);

    uint8_t mode    = buf[0] & 0x07;
    uint8_t version = (buf[0] >> 3) & 0x07;
    PROP_ASSERT_EQ(mode, 3);
    PROP_ASSERT_EQ(version, 4);
}

/* --- Property 4: random 48-byte buffer never crashes parse --- */

PROPERTY(parse_random_no_crash, 10000) {
    uint8_t buf[SNTP_PACKET_SIZE];
    ARBI_FILL(seed, buf, SNTP_PACKET_SIZE);

    sntp_response_t resp;
    /* may return error, but must not crash */
    sntp_client_parse_response(buf, SNTP_PACKET_SIZE, &resp);
    PROP_ASSERT(1);
}

/* --- Property 5: compute with equal timestamps yields zero offset --- */

PROPERTY(equal_timestamps_zero_offset, 5000) {
    uint32_t s = ARBI_UINT32(seed);
    /* ensure s > SNTP_UNIX_OFFSET to get valid unix time */
    if (s < (uint32_t)SNTP_UNIX_OFFSET) {
        s = (uint32_t)SNTP_UNIX_OFFSET + (s % 1000000);
    }

    uint8_t pkt[SNTP_PACKET_SIZE];
    build_valid_response(pkt, 2, s, s);

    sntp_response_t resp;
    sntp_client_err_t rc = sntp_client_parse_response(pkt, SNTP_PACKET_SIZE, &resp);
    PROP_ASSERT_EQ(rc, SNTP_CLIENT_OK);

    sntp_timestamp_t t1 = { .seconds = s, .fraction = 0 };
    sntp_timestamp_t t4 = { .seconds = s, .fraction = 0 };
    sntp_result_t result;
    rc = sntp_client_compute(&resp, &t1, &t4, &result);
    PROP_ASSERT_EQ(rc, SNTP_CLIENT_OK);
    PROP_ASSERT_EQ(result.offset_ms, 0);
    PROP_ASSERT_EQ(result.round_trip_ms, 0);
}

TEST_MAIN("sntp_client properties") {
    RUN(unix_ntp_roundtrip);
    RUN(request_always_48_bytes);
    RUN(request_mode_and_version);
    RUN(parse_random_no_crash);
    RUN(equal_timestamps_zero_offset);
}
TEST_MAIN_END()
