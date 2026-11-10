// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "sntp_client.h"
#include <string.h>

/* ─── helpers ────────────────────────────────────────────────── */

static void build_server_response(uint8_t *pkt, uint8_t stratum, uint8_t mode,
                                  uint32_t ref_s, uint32_t orig_s,
                                  uint32_t rx_s, uint32_t tx_s)
{
    memset(pkt, 0, SNTP_PACKET_SIZE);
    pkt[0] = (0 << 6) | (4 << 3) | mode;  /* LI=0, VN=4, Mode */
    pkt[1] = stratum;
    /* reference timestamp at offset 16 */
    pkt[16] = (uint8_t)(ref_s >> 24); pkt[17] = (uint8_t)(ref_s >> 16);
    pkt[18] = (uint8_t)(ref_s >>  8); pkt[19] = (uint8_t)(ref_s);
    /* originate timestamp at offset 24 */
    pkt[24] = (uint8_t)(orig_s >> 24); pkt[25] = (uint8_t)(orig_s >> 16);
    pkt[26] = (uint8_t)(orig_s >>  8); pkt[27] = (uint8_t)(orig_s);
    /* receive timestamp at offset 32 */
    pkt[32] = (uint8_t)(rx_s >> 24); pkt[33] = (uint8_t)(rx_s >> 16);
    pkt[34] = (uint8_t)(rx_s >>  8); pkt[35] = (uint8_t)(rx_s);
    /* transmit timestamp at offset 40 */
    pkt[40] = (uint8_t)(tx_s >> 24); pkt[41] = (uint8_t)(tx_s >> 16);
    pkt[42] = (uint8_t)(tx_s >>  8); pkt[43] = (uint8_t)(tx_s);
}

/* ─── build request tests ────────────────────────────────────── */

TEST(test_build_request_ok)
{
    uint8_t buf[SNTP_PACKET_SIZE];
    int rc = sntp_client_build_request(buf, sizeof(buf), NULL);
    ASSERT_EQ(rc, SNTP_PACKET_SIZE);
    ASSERT_EQ(buf[0] & 0x07, 3);      /* mode = client */
    ASSERT_EQ((buf[0] >> 3) & 0x07, 4); /* version = 4 */
}

TEST(test_build_request_with_tx_time)
{
    uint8_t buf[SNTP_PACKET_SIZE];
    sntp_timestamp_t tx = { .seconds = 0xAABBCCDD, .fraction = 0x11223344 };
    int rc = sntp_client_build_request(buf, sizeof(buf), &tx);
    ASSERT_EQ(rc, SNTP_PACKET_SIZE);
    ASSERT_EQ(buf[40], 0xAA);
    ASSERT_EQ(buf[41], 0xBB);
    ASSERT_EQ(buf[44], 0x11);
}

TEST(test_build_request_null)
{
    ASSERT_EQ(sntp_client_build_request(NULL, 48, NULL), SNTP_CLIENT_ERR_PARAM);
}

TEST(test_build_request_small_buf)
{
    uint8_t buf[10];
    ASSERT_EQ(sntp_client_build_request(buf, sizeof(buf), NULL), SNTP_CLIENT_ERR_SIZE);
}

/* ─── parse response tests ───────────────────────────────────── */

TEST(test_parse_server_response)
{
    uint8_t pkt[SNTP_PACKET_SIZE];
    build_server_response(pkt, 2, 4, 1000, 2000, 3000, 4000);
    sntp_response_t resp;
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_OK);
    ASSERT_EQ(resp.version, 4);
    ASSERT_EQ(resp.mode, 4);
    ASSERT_EQ(resp.stratum, 2);
    ASSERT_EQ(resp.receive.seconds, 3000);
    ASSERT_EQ(resp.transmit.seconds, 4000);
    ASSERT_EQ(resp.originate.seconds, 2000);
}

TEST(test_parse_broadcast_mode)
{
    uint8_t pkt[SNTP_PACKET_SIZE];
    build_server_response(pkt, 1, 5, 0, 0, 0, 1000); /* mode 5 = broadcast */
    sntp_response_t resp;
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_OK);
    ASSERT_EQ(resp.mode, 5);
}

TEST(test_parse_null)
{
    sntp_response_t resp;
    ASSERT_EQ(sntp_client_parse_response(NULL, 48, &resp), SNTP_CLIENT_ERR_PARAM);
    uint8_t pkt[48] = {0};
    ASSERT_EQ(sntp_client_parse_response(pkt, 48, NULL), SNTP_CLIENT_ERR_PARAM);
}

TEST(test_parse_too_short)
{
    uint8_t pkt[10] = {0};
    sntp_response_t resp;
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_ERR_SIZE);
}

TEST(test_parse_kiss_of_death)
{
    uint8_t pkt[SNTP_PACKET_SIZE];
    build_server_response(pkt, 0, 4, 0, 0, 0, 0); /* stratum 0 */
    sntp_response_t resp;
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_ERR_KISS);
}

TEST(test_parse_wrong_mode)
{
    uint8_t pkt[SNTP_PACKET_SIZE];
    build_server_response(pkt, 2, 3, 0, 0, 0, 0); /* mode 3 = client (wrong) */
    sntp_response_t resp;
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_ERR_MODE);
}

/* ─── what a client must not trust ───────────────────────────── */

/* RFC 4330 section 4: LI 3 is "alarm condition (clock not synchronized)", and
   strata 16-255 are reserved. Section 5: a reply's stratum is 0-15 and its
   Transmit Timestamp nonzero. Section 6: VN is copied from the request. */

static void set_li_vn(uint8_t *pkt, uint8_t li, uint8_t vn)
{
    pkt[0] = (uint8_t)((li << 6) | (vn << 3) | (pkt[0] & 0x07));
}

TEST(test_parse_refuses_a_server_that_says_it_is_not_synchronized)
{
    uint8_t pkt[SNTP_PACKET_SIZE];
    sntp_response_t resp;

    build_server_response(pkt, 2, 4, 1000, 2000, 3000, 4000);
    set_li_vn(pkt, 3, 4);
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp),
              SNTP_CLIENT_ERR_UNSYNC);

    /* 0 is no warning, 1 and 2 announce a leap second: all three carry time */
    for (uint8_t li = 0; li <= 2; li++) {
        set_li_vn(pkt, li, 4);
        ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_OK);
        ASSERT_EQ(resp.li, li);
    }
}

TEST(test_parse_refuses_the_all_zero_reply_of_a_starting_server)
{
    /* Section 6: unsynchronized or first coming up, all timestamps are zero
       with one exception, the copied Originate Timestamp */
    uint8_t pkt[SNTP_PACKET_SIZE];
    sntp_response_t resp;
    build_server_response(pkt, 2, 4, 0, 2000, 0, 0);
    set_li_vn(pkt, 3, 4);
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp),
              SNTP_CLIENT_ERR_UNSYNC);
}

TEST(test_parse_refuses_a_zero_transmit_timestamp)
{
    uint8_t pkt[SNTP_PACKET_SIZE];
    sntp_response_t resp;
    build_server_response(pkt, 2, 4, 1000, 2000, 3000, 0);
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp),
              SNTP_CLIENT_ERR_UNSYNC);

    /* Only all 64 bits at zero mean unavailable */
    pkt[47] = 1;
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_OK);
}

TEST(test_parse_refuses_a_reserved_stratum)
{
    static const uint8_t reserved[] = { 16, 17, 128, 255 };
    uint8_t pkt[SNTP_PACKET_SIZE];
    sntp_response_t resp;

    build_server_response(pkt, 1, 4, 1000, 2000, 3000, 4000);
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_OK);
    build_server_response(pkt, 15, 4, 1000, 2000, 3000, 4000);
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_OK);

    for (size_t i = 0; i < sizeof(reserved); i++) {
        build_server_response(pkt, reserved[i], 4, 1000, 2000, 3000, 4000);
        ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp),
                  SNTP_CLIENT_ERR_UNSYNC);
    }
}

TEST(test_parse_takes_only_versions_3_and_4)
{
    static const uint8_t refused[] = { 0, 1, 2, 5, 6, 7 };
    uint8_t pkt[SNTP_PACKET_SIZE];
    sntp_response_t resp;
    build_server_response(pkt, 2, 4, 1000, 2000, 3000, 4000);

    set_li_vn(pkt, 0, 3);
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_OK);
    set_li_vn(pkt, 0, 4);
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp), SNTP_CLIENT_OK);

    for (size_t i = 0; i < sizeof(refused); i++) {
        set_li_vn(pkt, 0, refused[i]);
        ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp),
                  SNTP_CLIENT_ERR_VERSION);
    }
}

TEST(test_parse_kiss_comes_before_every_other_check)
{
    /* Section 4: discard stratum 0 "regardless of the contents of other fields",
       and the caller has to see KISS to back off */
    uint8_t pkt[SNTP_PACKET_SIZE];
    sntp_response_t resp;
    build_server_response(pkt, 0, 4, 0, 0, 0, 0);
    set_li_vn(pkt, 3, 0);
    ASSERT_EQ(sntp_client_parse_response(pkt, sizeof(pkt), &resp),
              SNTP_CLIENT_ERR_KISS);
}

/* ─── compute tests ──────────────────────────────────────────── */

TEST(test_compute_zero_offset)
{
    /* T1=T2, T3=T4 → offset=0, rtt=0 */
    sntp_response_t resp = {
        .receive  = { .seconds = 100, .fraction = 0 },
        .transmit = { .seconds = 100, .fraction = 0 },
    };
    sntp_timestamp_t t1 = { .seconds = 100, .fraction = 0 };
    sntp_timestamp_t t4 = { .seconds = 100, .fraction = 0 };
    sntp_result_t result;
    ASSERT_EQ(sntp_client_compute(&resp, &t1, &t4, &result), SNTP_CLIENT_OK);
    ASSERT_EQ(result.offset_ms, 0);
    ASSERT_EQ(result.round_trip_ms, 0);
}

TEST(test_compute_positive_offset)
{
    /* Server is 2s ahead: T1=10s, T2=12s, T3=12s, T4=10s → offset = +2000ms */
    sntp_response_t resp = {
        .receive  = { .seconds = 12, .fraction = 0 },
        .transmit = { .seconds = 12, .fraction = 0 },
    };
    sntp_timestamp_t t1 = { .seconds = 10, .fraction = 0 };
    sntp_timestamp_t t4 = { .seconds = 10, .fraction = 0 };
    sntp_result_t result;
    sntp_client_compute(&resp, &t1, &t4, &result);
    ASSERT_EQ(result.offset_ms, 2000);
    ASSERT_EQ(result.round_trip_ms, 0);
}

TEST(test_compute_with_rtt)
{
    /* T1=10, T2=11, T3=11, T4=11 → offset=+500ms, rtt=1000ms */
    sntp_response_t resp = {
        .receive  = { .seconds = 11, .fraction = 0 },
        .transmit = { .seconds = 11, .fraction = 0 },
    };
    sntp_timestamp_t t1 = { .seconds = 10, .fraction = 0 };
    sntp_timestamp_t t4 = { .seconds = 11, .fraction = 0 };
    sntp_result_t result;
    sntp_client_compute(&resp, &t1, &t4, &result);
    ASSERT_EQ(result.offset_ms, 500);
    ASSERT_EQ(result.round_trip_ms, 1000);
}

TEST(test_compute_unix_time)
{
    uint32_t ntp_2024 = (uint32_t)SNTP_UNIX_OFFSET + 1700000000UL;
    sntp_response_t resp = {
        .receive  = { .seconds = ntp_2024, .fraction = 0 },
        .transmit = { .seconds = ntp_2024, .fraction = 0x80000000UL }, /* +500ms */
    };
    sntp_timestamp_t t1 = { .seconds = ntp_2024, .fraction = 0 };
    sntp_timestamp_t t4 = { .seconds = ntp_2024, .fraction = 0 };
    sntp_result_t result;
    sntp_client_compute(&resp, &t1, &t4, &result);
    ASSERT_EQ(result.unix_sec, 1700000000UL);
    ASSERT_TRUE(result.unix_ms >= 499 && result.unix_ms <= 500);
}

TEST(test_compute_null)
{
    sntp_response_t resp = {0};
    sntp_timestamp_t ts = {0};
    sntp_result_t r;
    ASSERT_EQ(sntp_client_compute(NULL, &ts, &ts, &r), SNTP_CLIENT_ERR_PARAM);
    ASSERT_EQ(sntp_client_compute(&resp, NULL, &ts, &r), SNTP_CLIENT_ERR_PARAM);
    ASSERT_EQ(sntp_client_compute(&resp, &ts, NULL, &r), SNTP_CLIENT_ERR_PARAM);
    ASSERT_EQ(sntp_client_compute(&resp, &ts, &ts, NULL), SNTP_CLIENT_ERR_PARAM);
}

/* ─── conversion tests ───────────────────────────────────────── */

TEST(test_unix_to_ntp_roundtrip)
{
    sntp_timestamp_t ntp = sntp_client_unix_to_ntp(1700000000UL, 500);
    uint32_t sec; uint16_t ms;
    sntp_client_ntp_to_unix(&ntp, &sec, &ms);
    ASSERT_EQ(sec, 1700000000UL);
    ASSERT_TRUE(ms >= 499 && ms <= 500);
}

TEST(test_ntp_to_unix_pre_epoch)
{
    /* Bit 0 set and below the offset is 1968-1970, which unix_sec cannot hold. */
    static const uint32_t seconds[] = { 0x80000000u, (uint32_t)SNTP_UNIX_OFFSET - 1u };
    for (size_t i = 0; i < sizeof(seconds) / sizeof(seconds[0]); i++) {
        sntp_timestamp_t ntp = { .seconds = seconds[i], .fraction = 0 };
        uint32_t sec = 1; uint16_t ms;
        sntp_client_ntp_to_unix(&ntp, &sec, &ms);
        ASSERT_EQ(sec, 0);
    }
}

/* RFC 4330 section 3: bit 0 clear means 2036-2104, reckoned from
 * 2036-02-07 06:28:16 UTC, which is Unix 2^32 - 2208988800 = 2085978496. */
TEST(test_ntp_to_unix_after_rollover)
{
    static const struct { uint32_t ntp; uint32_t unix_sec; } cases[] = {
        { 0xFFFFFFFFu, 2085978495u },   /* the last second of 1900-2036 */
        { 1u,          2085978497u },
        { 31536000u,   2117514496u },   /* 2037-02-06 06:28:16 UTC */
        { 1914021504u, 4000000000u },
        { 0x7FFFFFFFu, 4233462143u },   /* 2104-02-26, the last with bit 0 clear */
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        sntp_timestamp_t ntp = { .seconds = cases[i].ntp, .fraction = 0 };
        uint32_t sec = 0; uint16_t ms;
        sntp_client_ntp_to_unix(&ntp, &sec, &ms);
        ASSERT_EQ(sec, cases[i].unix_sec);

        sntp_timestamp_t back = sntp_client_unix_to_ntp(cases[i].unix_sec, 0);
        ASSERT_EQ(back.seconds, cases[i].ntp);
    }
}

/* RFC 4330 section 3: an all-zero timestamp means invalid or unavailable, but
 * a fraction makes it a real time just after the rollover. */
TEST(test_ntp_zero_timestamp_is_unavailable)
{
    sntp_timestamp_t zero = { .seconds = 0, .fraction = 0 };
    uint32_t sec = 1; uint16_t ms = 1;
    sntp_client_ntp_to_unix(&zero, &sec, &ms);
    ASSERT_EQ(sec, 0);
    ASSERT_EQ(ms, 0);

    sntp_timestamp_t half = { .seconds = 0, .fraction = 0x80000000u };
    sntp_client_ntp_to_unix(&half, &sec, &ms);
    ASSERT_EQ(sec, 2085978496u);
    ASSERT_EQ(ms, 500);
}

TEST(test_compute_across_rollover)
{
    /* T1 one tick before the seconds wrap, the rest just after it: the
     * client reads 1 s behind and the path takes 2 s. */
    sntp_response_t resp = {
        .receive  = { .seconds = 1, .fraction = 0 },
        .transmit = { .seconds = 1, .fraction = 0 },
    };
    sntp_timestamp_t t1 = { .seconds = 0xFFFFFFFFu, .fraction = 0 };
    sntp_timestamp_t t4 = { .seconds = 1, .fraction = 0 };
    sntp_result_t result;
    ASSERT_EQ(sntp_client_compute(&resp, &t1, &t4, &result), SNTP_CLIENT_OK);
    ASSERT_EQ(result.offset_ms, 1000);
    ASSERT_EQ(result.round_trip_ms, 2000);
    ASSERT_EQ(result.unix_sec, 2085978497u);

    /* Server past the wrap, client not: +5 s. */
    resp.receive.seconds = resp.transmit.seconds = 3;
    t1.seconds = t4.seconds = 0xFFFFFFFEu;
    sntp_client_compute(&resp, &t1, &t4, &result);
    ASSERT_EQ(result.offset_ms, 5000);
    ASSERT_EQ(result.round_trip_ms, 0);

    /* Client past the wrap, server not: -5 s. */
    resp.receive.seconds = resp.transmit.seconds = 0xFFFFFFFEu;
    t1.seconds = t4.seconds = 3;
    sntp_client_compute(&resp, &t1, &t4, &result);
    ASSERT_EQ(result.offset_ms, -5000);
    ASSERT_EQ(result.round_trip_ms, 0);

    /* With fractions: T2-T1 = +1 s, T3-T4 = -0.5 s, T4-T1 = 2 s, T3-T2 = 0.5 s. */
    t1 = (sntp_timestamp_t){ .seconds = 0xFFFFFFFFu, .fraction = 0x80000000u };
    resp.receive  = (sntp_timestamp_t){ .seconds = 0, .fraction = 0x80000000u };
    resp.transmit = (sntp_timestamp_t){ .seconds = 1, .fraction = 0 };
    t4 = (sntp_timestamp_t){ .seconds = 1, .fraction = 0x80000000u };
    sntp_client_compute(&resp, &t1, &t4, &result);
    ASSERT_EQ(result.offset_ms, 250);
    ASSERT_EQ(result.round_trip_ms, 1500);
}

TEST(test_ntp_to_unix_null)
{
    uint32_t sec; uint16_t ms;
    sntp_client_ntp_to_unix(NULL, &sec, &ms);
    /* should not crash */
}

TEST(test_unix_to_ntp_zero)
{
    sntp_timestamp_t ntp = sntp_client_unix_to_ntp(0, 0);
    ASSERT_EQ(ntp.seconds, (uint32_t)SNTP_UNIX_OFFSET);
    ASSERT_EQ(ntp.fraction, 0);
}

/* ─── fuzz ───────────────────────────────────────────────────── */

TEST(test_parse_fuzz)
{
    sntp_response_t resp;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[64];
        for (int j = 0; j < 48; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        sntp_client_parse_response(buf, 48, &resp);
    }
}

TEST(test_ms_survive_the_round_trip)
{
    for (uint16_t ms = 0; ms < 1000; ms++) {
        sntp_timestamp_t ntp = sntp_client_unix_to_ntp(1700000000U, ms);
        uint32_t sec = 0;
        uint16_t back = 0;
        sntp_client_ntp_to_unix(&ntp, &sec, &back);
        ASSERT_EQ(sec, 1700000000U);
        ASSERT_EQ(back, ms);
    }
}

TEST_MAIN("sntp_client")
    RUN(test_build_request_ok);
    RUN(test_build_request_with_tx_time);
    RUN(test_build_request_null);
    RUN(test_build_request_small_buf);
    RUN(test_parse_server_response);
    RUN(test_parse_broadcast_mode);
    RUN(test_parse_null);
    RUN(test_parse_too_short);
    RUN(test_parse_kiss_of_death);
    RUN(test_parse_wrong_mode);
    RUN(test_parse_refuses_a_server_that_says_it_is_not_synchronized);
    RUN(test_parse_refuses_the_all_zero_reply_of_a_starting_server);
    RUN(test_parse_refuses_a_zero_transmit_timestamp);
    RUN(test_parse_refuses_a_reserved_stratum);
    RUN(test_parse_takes_only_versions_3_and_4);
    RUN(test_parse_kiss_comes_before_every_other_check);
    RUN(test_compute_zero_offset);
    RUN(test_compute_positive_offset);
    RUN(test_compute_with_rtt);
    RUN(test_compute_unix_time);
    RUN(test_compute_null);
    RUN(test_unix_to_ntp_roundtrip);
    RUN(test_ntp_to_unix_pre_epoch);
    RUN(test_ntp_to_unix_after_rollover);
    RUN(test_ntp_zero_timestamp_is_unavailable);
    RUN(test_compute_across_rollover);
    RUN(test_ntp_to_unix_null);
    RUN(test_unix_to_ntp_zero);
    RUN(test_parse_fuzz);
    RUN(test_ms_survive_the_round_trip);
TEST_MAIN_END()
