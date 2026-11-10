// SPDX-License-Identifier: MIT
/*
 * sntp_client — SNTP v4 packet codec + clock offset calculation
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 */
#include "sntp_client.h"
#include "contract.h"
#include <string.h>

/* ─── byte helpers (big-endian) ──────────────────────────────── */

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
}

static sntp_timestamp_t rd_ts(const uint8_t *p)
{
    sntp_timestamp_t ts;
    ts.seconds  = rd32(p);
    ts.fraction = rd32(p + 4);
    return ts;
}

static void wr_ts(uint8_t *p, const sntp_timestamp_t *ts)
{
    wr32(p,     ts->seconds);
    wr32(p + 4, ts->fraction);
}

/* ─── timestamp math ─────────────────────────────────────────── */

static int64_t fraction_to_ms(uint32_t fraction)
{
    return ((int64_t)fraction * 1000LL) >> 32;
}

/* a - b in ms. The seconds differ modulo 2^32, so two stamps either side of
 * the 2036 rollover stay close; that holds up to 68 years either way. */
static int64_t ts_diff_ms(const sntp_timestamp_t *a, const sntp_timestamp_t *b)
{
    uint32_t d = a->seconds - b->seconds;
    int64_t s  = (d & 0x80000000u) ? (int64_t)d - 0x100000000LL : (int64_t)d;
    return s * 1000LL + (fraction_to_ms(a->fraction) - fraction_to_ms(b->fraction));
}

/* RFC 4330 section 3: bit 0 set is 1968-2036, reckoned from 1900; bit 0 clear
 * is 2036-2104, reckoned from 2036-02-07, and the subtraction wraps to the
 * right value for both. 1968-1970 does not fit unix_sec and an all-zero
 * timestamp means unavailable: both read as 0. */
static uint32_t ts_to_unix_sec(const sntp_timestamp_t *ts)
{
    if (ts->seconds == 0 && ts->fraction == 0) return 0;
    if ((ts->seconds & 0x80000000u) && ts->seconds < (uint32_t)SNTP_UNIX_OFFSET)
        return 0;
    return ts->seconds - (uint32_t)SNTP_UNIX_OFFSET;
}

/* ─── public API ─────────────────────────────────────────────── */

int sntp_client_build_request(uint8_t *buf, size_t buf_size,
                              const sntp_timestamp_t *tx_time)
{
    REQUIRE(buf != NULL);
    if (!buf) return (int)SNTP_CLIENT_ERR_PARAM;
    REQUIRE(buf_size >= SNTP_PACKET_SIZE);
    if (buf_size < SNTP_PACKET_SIZE) return (int)SNTP_CLIENT_ERR_SIZE;

    memset(buf, 0, SNTP_PACKET_SIZE);
    buf[0] = (0 << 6) |   /* LI = 0 (no warning) */
             (4 << 3) |   /* VN = 4 (SNTPv4) */
             (3);          /* Mode = 3 (client) */

    if (tx_time) {
        wr_ts(&buf[40], tx_time);
    }

    return SNTP_PACKET_SIZE;
}

sntp_client_err_t sntp_client_parse_response(const uint8_t *data, size_t len,
                                             sntp_response_t *out)
{
    REQUIRE(data != NULL);
    REQUIRE(out != NULL);
    if (!data || !out) return SNTP_CLIENT_ERR_PARAM;
    if (len < SNTP_PACKET_SIZE) return SNTP_CLIENT_ERR_SIZE;

    out->li        = (data[0] >> 6) & 0x03;
    out->version   = (data[0] >> 3) & 0x07;
    out->mode      = data[0] & 0x07;
    out->stratum   = data[1];
    out->precision  = (int8_t)data[3];

    if (out->stratum == 0) return SNTP_CLIENT_ERR_KISS;
    if (out->mode != 4 && out->mode != 5) return SNTP_CLIENT_ERR_MODE;

    out->reference = rd_ts(&data[16]);
    out->originate = rd_ts(&data[24]);
    out->receive   = rd_ts(&data[32]);
    out->transmit  = rd_ts(&data[40]);

    if (out->version != 3 && out->version != 4) return SNTP_CLIENT_ERR_VERSION;

    /* RFC 4330 section 4: LI 3 is "clock not synchronized", strata from 16 are
     * reserved, and section 5 wants a nonzero Transmit Timestamp. Check 4 there
     * prints "LI" among the fields that must not be 0, but its own table allows
     * 0-3 and 0 is every healthy server, so only 3 is refused. */
    if (out->li == 3 || out->stratum > 15) return SNTP_CLIENT_ERR_UNSYNC;
    if (out->transmit.seconds == 0 && out->transmit.fraction == 0)
        return SNTP_CLIENT_ERR_UNSYNC;

    return SNTP_CLIENT_OK;
}

sntp_client_err_t sntp_client_compute(const sntp_response_t *resp,
                                      const sntp_timestamp_t *t1,
                                      const sntp_timestamp_t *t4,
                                      sntp_result_t *out)
{
    REQUIRE(resp != NULL);
    REQUIRE(t1 != NULL);
    REQUIRE(t4 != NULL);
    REQUIRE(out != NULL);
    if (!resp || !t1 || !t4 || !out) return SNTP_CLIENT_ERR_PARAM;

    /* offset = ((T2 - T1) + (T3 - T4)) / 2 */
    out->offset_ms = (ts_diff_ms(&resp->receive, t1) +
                      ts_diff_ms(&resp->transmit, t4)) / 2;

    /* round_trip = (T4 - T1) - (T3 - T2) */
    int64_t rt = ts_diff_ms(t4, t1) - ts_diff_ms(&resp->transmit, &resp->receive);
    out->round_trip_ms = (rt > 0) ? (uint32_t)rt : 0;

    /* server time = T3 adjusted to Unix epoch */
    out->unix_sec = ts_to_unix_sec(&resp->transmit);
    out->unix_ms = (uint16_t)(((uint64_t)resp->transmit.fraction * 1000ULL) >> 32);

    return SNTP_CLIENT_OK;
}

sntp_timestamp_t sntp_client_unix_to_ntp(uint32_t unix_sec, uint16_t ms)
{
    sntp_timestamp_t ts;
    ts.seconds  = unix_sec + (uint32_t)SNTP_UNIX_OFFSET;
    /* Round up: the way back floors, so a floored fraction would lose 1 ms. */
    ts.fraction = (uint32_t)((((uint64_t)ms << 32) + 999ULL) / 1000ULL);
    return ts;
}

void sntp_client_ntp_to_unix(const sntp_timestamp_t *ntp,
                      uint32_t *unix_sec, uint16_t *ms)
{
    REQUIRE(ntp != NULL);
    REQUIRE(unix_sec != NULL);
    REQUIRE(ms != NULL);
    if (!ntp || !unix_sec || !ms) return;
    *unix_sec = ts_to_unix_sec(ntp);
    *ms =(uint16_t)(((uint64_t)ntp->fraction * 1000ULL) >> 32);
}
