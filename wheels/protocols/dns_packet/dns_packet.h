// SPDX-License-Identifier: MIT
/*
 * dns_packet — DNS query/response encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless DNS packet building and parsing. Handles query construction,
 * header/record parsing, domain name encoding with pointer compression.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef DNS_PACKET_H
#define DNS_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DNS_NAME_MAX
#define DNS_NAME_MAX 253
#endif

#define DNS_HEADER_SIZE 12

typedef enum {
    DNS_OK           =  0,
    DNS_ERR_PARAM   = -1,
    DNS_ERR_SIZE     = -2,
    DNS_ERR_FORMAT   = -3,
    DNS_ERR_OVERFLOW = -4,
} dns_err_t;

typedef enum {
    DNS_TYPE_A     = 1,
    DNS_TYPE_CNAME = 5,
    DNS_TYPE_PTR   = 12,
    DNS_TYPE_MX    = 15,
    DNS_TYPE_TXT   = 16,
    DNS_TYPE_AAAA  = 28,
} dns_type_t;

typedef enum {
    DNS_CLASS_IN = 1,
} dns_class_t;

typedef struct {
    uint16_t id;
    bool     is_response;
    uint8_t  rcode;         /* 0=NOERROR, 3=NXDOMAIN */
    uint16_t qdcount;
    uint16_t ancount;
} dns_header_t;

typedef struct {
    char           name[DNS_NAME_MAX];
    dns_type_t     type;
    dns_class_t    cls;
    uint32_t       ttl;         /* only meaningful in answers */
    const uint8_t *rdata;       /* points into input buffer */
    uint16_t       rdlength;
} dns_record_t;

/**
 * Build a DNS query for a domain name.
 * Returns bytes written on success, or negative dns_err_t on failure.
 * @thread_safe: yes (pure function)
 */
int dns_build_query(uint8_t *buf, uint16_t buf_size,
                    uint16_t id, const char *name, dns_type_t type);

/**
 * Parse DNS header from raw packet.
 * @thread_safe: yes (pure function)
 */
dns_err_t dns_parse_header(const uint8_t *buf, uint16_t len,
                           dns_header_t *hdr);

/**
 * Parse a record (question or answer) starting at offset.
 * Returns new offset after parsing, or negative dns_err_t on error.
 * @thread_safe: yes (pure function)
 */
int dns_parse_record(const uint8_t *buf, uint16_t len, uint16_t offset,
                     dns_record_t *record, bool is_answer);

/**
 * Extract IPv4 address from an A record's rdata.
 * @thread_safe: yes (pure function)
 */
dns_err_t dns_get_ipv4(const dns_record_t *record, uint8_t ip[4]);

/**
 * Encode domain name to DNS wire format (label-length encoding).
 * Returns bytes written on success, or negative dns_err_t on error.
 * @thread_safe: yes (pure function)
 */
int dns_encode_name(const char *name, uint8_t *buf, uint16_t buf_size);

/**
 * Decode domain name from DNS wire format (handles pointer compression).
 * Returns the number of bytes consumed from the packet at offset,
 * or negative dns_err_t on error.
 * @thread_safe: yes (pure function)
 */
int dns_decode_name(const uint8_t *pkt, uint16_t pkt_len,
                    uint16_t offset, char *name, uint16_t name_size);

#ifdef __cplusplus
}
#endif

#endif /* DNS_PACKET_H */
