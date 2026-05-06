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

static int64_t ts_to_ms(const sntp_timestamp_t *ts)
{
    int64_t s  = (int64_t)ts->seconds;
    int64_t ms = ((int64_t)ts->fraction * 1000LL) >> 32;
    return s * 1000LL + ms;
}

/* ─── public API ─────────────────────────────────────────────── */

int sntp_client_build_request(uint8_t *buf, size_t buf_size,
                              const sntp_timestamp_t *tx_time)
{
    if (!buf) return (int)SNTP_CLIENT_ERR_PARAM;
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

    return SNTP_CLIENT_OK;
}

sntp_client_err_t sntp_client_compute(const sntp_response_t *resp,
                                      const sntp_timestamp_t *t1,
                                      const sntp_timestamp_t *t4,
                                      sntp_result_t *out)
{
    if (!resp || !t1 || !t4 || !out) return SNTP_CLIENT_ERR_PARAM;

    int64_t t1_ms = ts_to_ms(t1);
    int64_t t2_ms = ts_to_ms(&resp->receive);
    int64_t t3_ms = ts_to_ms(&resp->transmit);
    int64_t t4_ms = ts_to_ms(t4);

    /* offset = ((T2 - T1) + (T3 - T4)) / 2 */
    out->offset_ms = ((t2_ms - t1_ms) + (t3_ms - t4_ms)) / 2;

    /* round_trip = (T4 - T1) - (T3 - T2) */
    int64_t rt = (t4_ms - t1_ms) - (t3_ms - t2_ms);
    out->round_trip_ms = (rt > 0) ? (uint32_t)rt : 0;

    /* server time = T3 adjusted to Unix epoch */
    if (resp->transmit.seconds >= (uint32_t)SNTP_UNIX_OFFSET) {
        out->unix_sec = resp->transmit.seconds - (uint32_t)SNTP_UNIX_OFFSET;
    } else {
        out->unix_sec = 0;
    }
    out->unix_ms = (uint16_t)(((uint64_t)resp->transmit.fraction * 1000ULL) >> 32);

    return SNTP_CLIENT_OK;
}

sntp_timestamp_t sntp_client_unix_to_ntp(uint32_t unix_sec, uint16_t ms)
{
    sntp_timestamp_t ts;
    ts.seconds  = unix_sec + (uint32_t)SNTP_UNIX_OFFSET;
    ts.fraction = (uint32_t)(((uint64_t)ms << 32) / 1000ULL);
    return ts;
}

void sntp_client_ntp_to_unix(const sntp_timestamp_t *ntp,
                      uint32_t *unix_sec, uint16_t *ms)
{
    if (!ntp || !unix_sec || !ms) return;
    if (ntp->seconds >= (uint32_t)SNTP_UNIX_OFFSET) {
        *unix_sec = ntp->seconds - (uint32_t)SNTP_UNIX_OFFSET;
    } else {
        *unix_sec = 0;
    }
    *ms = (uint16_t)(((uint64_t)ntp->fraction * 1000ULL) >> 32);
}
