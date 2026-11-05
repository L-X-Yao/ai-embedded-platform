// SPDX-License-Identifier: MIT
/*
 * dhcp_packet — DHCP message builder/parser (RFC 2131)
 *
 * Layer:     L2
 * Status:    candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless DHCP packet building and parsing. Handles DISCOVER/OFFER/
 * REQUEST/ACK message types, option iteration, and common option
 * extraction. Pure C99, no heap. All buffers caller-provided.
 */

#ifndef DHCP_PACKET_H
#define DHCP_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */

#define DHCP_PACKET_MIN_LEN    240
#define DHCP_PACKET_BOOTP_MIN_LEN 300  /* RFC 1542: builders pad up to this */
#define DHCP_PACKET_MAX_LEN    576
#define DHCP_MAGIC_COOKIE      0x63825363U

/* ---------- error codes ---------- */

typedef enum {
    DHCP_PACKET_OK           =  0,
    DHCP_PACKET_ERR_PARAM    = -1,
    DHCP_PACKET_ERR_FORMAT   = -2,
    DHCP_PACKET_ERR_SHORT    = -3,
    DHCP_PACKET_ERR_OVERFLOW = -4,
} dhcp_packet_err_t;

/* ---------- enums ---------- */

typedef enum {
    DHCP_OP_REQUEST  = 1,
    DHCP_OP_REPLY    = 2,
} dhcp_op_t;

typedef enum {
    DHCP_MSG_DISCOVER = 1,
    DHCP_MSG_OFFER    = 2,
    DHCP_MSG_REQUEST  = 3,
    DHCP_MSG_DECLINE  = 4,
    DHCP_MSG_ACK      = 5,
    DHCP_MSG_NAK      = 6,
    DHCP_MSG_RELEASE  = 7,
    DHCP_MSG_INFORM   = 8,
} dhcp_msg_type_t;

typedef enum {
    DHCP_OPT_PAD            = 0,
    DHCP_OPT_SUBNET_MASK    = 1,
    DHCP_OPT_ROUTER         = 3,
    DHCP_OPT_DNS            = 6,
    DHCP_OPT_HOSTNAME       = 12,
    DHCP_OPT_REQ_IP         = 50,
    DHCP_OPT_LEASE_TIME     = 51,
    DHCP_OPT_MSG_TYPE       = 53,
    DHCP_OPT_SERVER_ID      = 54,
    DHCP_OPT_PARAM_LIST     = 55,
    DHCP_OPT_END            = 255,
} dhcp_opt_t;

/* ---------- data types ---------- */

typedef struct {
    uint8_t  op;           /* 1=request, 2=reply */
    uint8_t  htype;        /* 1=Ethernet */
    uint8_t  hlen;         /* 6 for Ethernet */
    uint8_t  hops;
    uint32_t xid;          /* transaction ID */
    uint16_t secs;
    uint16_t flags;
    uint8_t  ciaddr[4];    /* client IP */
    uint8_t  yiaddr[4];    /* 'your' IP */
    uint8_t  siaddr[4];    /* server IP */
    uint8_t  giaddr[4];    /* relay agent IP */
    uint8_t  chaddr[16];   /* client hardware addr */
    uint8_t  msg_type;     /* parsed from option 53 */
} dhcp_header_t;

typedef struct {
    uint8_t        code;
    uint8_t        len;
    const uint8_t *data;
} dhcp_option_t;

/* ---------- API ---------- */

/**
 * Parse DHCP packet header (fixed fields + msg_type from options).
 * @thread_safe: yes (pure function)
 */
int dhcp_packet_parse(const uint8_t *buf, size_t len, dhcp_header_t *hdr);

/**
 * Iterate options. Call repeatedly with offset starting at 240 (options start).
 * Returns 1 when an option is found, 0 when done (END or past buffer),
 * -1 on error.
 * @thread_safe: yes (pure function)
 */
int dhcp_packet_next_option(const uint8_t *buf, size_t len,
                            size_t *offset, dhcp_option_t *opt);

/**
 * Get a specific option by code.
 * Returns 0 on success, -1 if not found or error.
 * @thread_safe: yes (pure function)
 */
int dhcp_packet_get_option(const uint8_t *buf, size_t len,
                           uint8_t code, dhcp_option_t *opt);

/**
 * Build a DHCP DISCOVER message, zero-padded to DHCP_PACKET_BOOTP_MIN_LEN.
 * Returns bytes written on success, or negative dhcp_packet_err_t on failure.
 * A cap below DHCP_PACKET_BOOTP_MIN_LEN returns DHCP_PACKET_ERR_OVERFLOW.
 * @thread_safe: yes (pure function)
 */
int dhcp_packet_build_discover(uint8_t *buf, size_t cap,
                               uint32_t xid, const uint8_t mac[6]);

/**
 * Build a DHCP REQUEST message, zero-padded to DHCP_PACKET_BOOTP_MIN_LEN.
 * Returns bytes written on success, or negative dhcp_packet_err_t on failure.
 * A cap below DHCP_PACKET_BOOTP_MIN_LEN returns DHCP_PACKET_ERR_OVERFLOW.
 * @thread_safe: yes (pure function)
 */
int dhcp_packet_build_request(uint8_t *buf, size_t cap,
                              uint32_t xid, const uint8_t mac[6],
                              const uint8_t req_ip[4],
                              const uint8_t server_ip[4]);

/**
 * Get uint32 from option data (network byte order -> host).
 * @thread_safe: yes (pure function)
 */
uint32_t dhcp_packet_option_u32(const dhcp_option_t *opt);

/**
 * Get IPv4 address from option data.
 * @thread_safe: yes (pure function)
 */
void dhcp_packet_option_ip(const dhcp_option_t *opt, uint8_t ip[4]);

#ifdef __cplusplus
}
#endif

#endif /* DHCP_PACKET_H */
