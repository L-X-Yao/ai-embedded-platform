// SPDX-License-Identifier: MIT
/*
 * sntp_client — SNTP v4 packet codec + clock offset calculation
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Builds a 48-byte SNTP request, parses a 48-byte response, and
 * computes the clock offset / round-trip delay per RFC 4330.
 * No transport — caller handles UDP send/receive.
 * Pure C99, no heap, fully reentrant.
 */

#ifndef SNTP_CLIENT_H
#define SNTP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define SNTP_PACKET_SIZE    48
#define SNTP_UNIX_OFFSET    2208988800ULL  /* seconds from 1900-01-01 to 1970-01-01 */

/* ---------- error codes ---------- */

typedef enum {
    SNTP_CLIENT_OK           =  0,
    SNTP_CLIENT_ERR_PARAM    = -1,
    SNTP_CLIENT_ERR_SIZE     = -2,
    SNTP_CLIENT_ERR_KISS     = -3,  /* Kiss-of-Death: stratum 0 */
    SNTP_CLIENT_ERR_MODE     = -4,  /* unexpected mode in response */
    SNTP_CLIENT_ERR_UNSYNC   = -5,  /* server has no good time to give: LI 3,
                                     * stratum above 15 or zero Transmit Timestamp */
    SNTP_CLIENT_ERR_VERSION  = -6,  /* version other than 3 or 4 in response */
    SNTP_CLIENT_ERR_ORIGINATE = -7, /* reply does not answer this request */
} sntp_client_err_t;

/* ---------- NTP timestamp (64-bit: 32 seconds + 32 fraction) ---------- */

typedef struct {
    uint32_t seconds;
    uint32_t fraction;
} sntp_timestamp_t;

/* ---------- parsed response ---------- */

typedef struct {
    uint8_t          li;           /* leap indicator (0-3) */
    uint8_t          version;      /* NTP version (3 or 4) */
    uint8_t          mode;         /* mode (4 = server) */
    uint8_t          stratum;      /* stratum (1 = primary, 2+ = secondary) */
    int8_t           precision;    /* log2(seconds) of clock precision */
    sntp_timestamp_t reference;    /* last sync time */
    sntp_timestamp_t originate;    /* T1: client transmit (echoed back) */
    sntp_timestamp_t receive;      /* T2: server receive */
    sntp_timestamp_t transmit;     /* T3: server transmit */
} sntp_response_t;

/* ---------- clock result ---------- */

typedef struct {
    int64_t  offset_ms;      /* positive = local behind: add to the local clock */
    uint32_t round_trip_ms;  /* network round-trip delay */
    uint32_t unix_sec;       /* server time as Unix timestamp (seconds since 1970) */
    uint16_t unix_ms;        /* fractional milliseconds of server time */
} sntp_result_t;

/* ---------- API ---------- */

/**
 * Build a 48-byte SNTP request packet.
 * @param buf        Output buffer (must be >= SNTP_PACKET_SIZE)
 * @param buf_size   Buffer capacity
 * @param tx_time    Client transmit timestamp from the local clock, which need
 *                   not be synchronized. Pass the t1 you will give
 *                   sntp_client_compute(): the server copies it into its reply,
 *                   and that is the only proof the reply answers this request.
 *                   With NULL or zero, compute refuses the reply, and only the
 *                   server's transmit time can be read from it.
 * @return SNTP_PACKET_SIZE on success, negative on error
 * @thread_safe: yes (pure) @isr_safe: yes
 */
int sntp_client_build_request(uint8_t *buf, size_t buf_size,
                              const sntp_timestamp_t *tx_time);

/**
 * Parse a 48-byte SNTP response into structured fields, and refuse one a client
 * must not take time from (RFC 4330 sections 4 and 5). Stratum 0 is reported
 * first, as SNTP_CLIENT_ERR_KISS, whatever else the packet holds.
 * After SNTP_CLIENT_ERR_UNSYNC or SNTP_CLIENT_ERR_VERSION out holds every field,
 * for a log, but its time must not be used.
 * @thread_safe: yes (pure) @isr_safe: yes
 */
sntp_client_err_t sntp_client_parse_response(const uint8_t *data, size_t len,
                                             sntp_response_t *out);

/**
 * Compute clock offset and round-trip delay.
 * @param resp   Parsed server response (contains T2, T3)
 * @param t1     Client originate timestamp (when request was sent), the value
 *               given to sntp_client_build_request()
 * @param t4     Client destination timestamp (when response arrived)
 * @param out    Result with offset, round_trip, and unix time
 *
 * Returns SNTP_CLIENT_ERR_ORIGINATE, and leaves out alone, unless resp->originate
 * equals t1 (RFC 4330 section 5): the reply is then stale, forged or meant for
 * another request. A broadcast has no request and carries zero, so it takes a
 * zero t1, and then only unix_sec and unix_ms mean anything.
 *
 * Differences are taken modulo 2^32 seconds (RFC 4330), so they hold across
 * the 2036 rollover as long as the clocks agree to within 68 years.
 * @thread_safe: yes (pure) @isr_safe: yes
 */
sntp_client_err_t sntp_client_compute(const sntp_response_t *resp,
                                      const sntp_timestamp_t *t1,
                                      const sntp_timestamp_t *t4,
                                      sntp_result_t *out);

/**
 * Convert Unix timestamp (seconds + ms) to NTP timestamp.
 * @thread_safe: yes (pure) @isr_safe: yes
 */
sntp_timestamp_t sntp_client_unix_to_ntp(uint32_t unix_sec, uint16_t ms);

/**
 * Convert NTP timestamp to Unix seconds + milliseconds.
 * RFC 4330 section 3: seconds with bit 0 clear are 2036-2104. unix_sec is 0
 * for 1968-1970, which it cannot hold, and for the all-zero timestamp.
 * @thread_safe: yes (pure) @isr_safe: yes
 */
void sntp_client_ntp_to_unix(const sntp_timestamp_t *ntp,
                      uint32_t *unix_sec, uint16_t *ms);

#ifdef __cplusplus
}
#endif

#endif /* SNTP_CLIENT_H */
