// SPDX-License-Identifier: MIT
/*
 * coap_packet — CoAP message encoder/decoder (RFC 7252)
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless packet-level codec for CoAP over UDP.
 * Supports CON/NON/ACK/RST types, options, payload marker.
 * Pure C99, no heap, zero-copy parsing.
 * Thread safety: all functions are pure / reentrant (no global state).
 */

#ifndef COAP_PACKET_H
#define COAP_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COAP_PACKET_OK             =  0,
    COAP_PACKET_ERR_PARAM      = -1,
    COAP_PACKET_ERR_INCOMPLETE = -2,
    COAP_PACKET_ERR_MALFORMED  = -3,
    COAP_PACKET_ERR_OVERFLOW   = -4,
    COAP_PACKET_ERR_TOO_MANY   = -5,
} coap_packet_err_t;

typedef enum {
    COAP_TYPE_CON = 0,
    COAP_TYPE_NON = 1,
    COAP_TYPE_ACK = 2,
    COAP_TYPE_RST = 3,
} coap_packet_type_t;

typedef enum {
    COAP_METHOD_GET    = 1,
    COAP_METHOD_POST   = 2,
    COAP_METHOD_PUT    = 3,
    COAP_METHOD_DELETE = 4,
} coap_packet_method_t;

#define COAP_CODE(cls, detail) (((cls) << 5) | (detail))
#define COAP_CODE_EMPTY          COAP_CODE(0, 0)
#define COAP_CODE_GET            COAP_CODE(0, 1)
#define COAP_CODE_POST           COAP_CODE(0, 2)
#define COAP_CODE_PUT            COAP_CODE(0, 3)
#define COAP_CODE_DELETE         COAP_CODE(0, 4)
#define COAP_CODE_CREATED        COAP_CODE(2, 1)
#define COAP_CODE_CONTENT        COAP_CODE(2, 5)
#define COAP_CODE_BAD_REQUEST    COAP_CODE(4, 0)
#define COAP_CODE_NOT_FOUND      COAP_CODE(4, 4)
#define COAP_CODE_INTERNAL_ERR   COAP_CODE(5, 0)

#define COAP_OPT_URI_HOST     3
#define COAP_OPT_URI_PORT     7
#define COAP_OPT_URI_PATH    11
#define COAP_OPT_CONTENT_FMT 12
#define COAP_OPT_URI_QUERY   15

#ifndef COAP_MAX_OPTIONS
#define COAP_MAX_OPTIONS 8
#endif

typedef struct {
    uint16_t       number;
    const uint8_t *value;
    size_t         value_len;
} coap_packet_option_t;

typedef struct {
    uint8_t        version;
    coap_packet_type_t    type;
    uint8_t        code;
    uint16_t       message_id;
    const uint8_t *token;
    uint8_t        token_len;
    coap_packet_option_t  options[COAP_MAX_OPTIONS];
    int            option_count;
    const uint8_t *payload;
    size_t         payload_len;
    size_t         total_len;
} coap_packet_t;

/** @thread_safe: yes (pure function, no state) */
coap_packet_err_t coap_packet_parse(const uint8_t *data, size_t len, coap_packet_t *pkt);

coap_packet_err_t coap_packet_build(uint8_t *buf, size_t buf_len, size_t *out_len,
                      coap_packet_type_t type, uint8_t code, uint16_t message_id,
                      const uint8_t *token, uint8_t token_len,
                      const coap_packet_option_t *options, int option_count,
                      const uint8_t *payload, size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif /* COAP_PACKET_H */
