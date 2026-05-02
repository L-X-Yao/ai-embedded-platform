// SPDX-License-Identifier: MIT
/*
 * mqtt5_packet — MQTT 5.0 packet encoder/decoder
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless packet building and parsing for MQTT 5.0. Handles CONNECT,
 * PUBLISH, SUBSCRIBE, DISCONNECT encoding and CONNACK, PUBLISH decoding.
 * Extends mqtt_packet (v3.1.1) with properties, reason codes, and
 * variable byte integer helpers. No transport — caller handles TCP/TLS.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef MQTT5_PACKET_H
#define MQTT5_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MQTT5_OK            =  0,
    MQTT5_ERR_PARAM    = -1,
    MQTT5_ERR_SIZE      = -2,
    MQTT5_ERR_PROTOCOL  = -3,
    MQTT5_ERR_OVERFLOW  = -4,
} mqtt5_err_t;

/* MQTT 5.0 property identifiers (subset used by this codec) */
#define MQTT5_PROP_SESSION_EXPIRY   0x11  /* 4-byte */
#define MQTT5_PROP_RECEIVE_MAX      0x21  /* 2-byte */
#define MQTT5_PROP_MAX_QOS          0x24  /* 1-byte */
#define MQTT5_PROP_MESSAGE_EXPIRY   0x02  /* 4-byte */

typedef struct {
    uint16_t    keep_alive;
    const char *client_id;
    const char *username;
    const char *password;
    bool        clean_start;
    uint32_t    session_expiry;
} mqtt5_connect_t;

typedef struct {
    uint8_t     reason_code;
    bool        session_present;
    uint32_t    session_expiry;
    uint16_t    receive_max;
    uint8_t     max_qos;
} mqtt5_connack_t;

typedef struct {
    const char    *topic;
    const uint8_t *payload;
    uint16_t       payload_len;
    uint16_t       packet_id;
    uint8_t        qos;
    bool           retain;
    uint32_t       message_expiry;
} mqtt5_publish_t;

/** Build CONNECT packet (v5.0). Returns bytes written or negative mqtt5_err_t.
 *  @thread_safe: yes (pure function, no state) */
int mqtt5_build_connect(uint8_t *buf, uint16_t size,
                        const mqtt5_connect_t *cfg);

/** Build PUBLISH packet (v5.0). Returns bytes written or negative mqtt5_err_t.
 *  @thread_safe: yes (pure function) */
int mqtt5_build_publish(uint8_t *buf, uint16_t size,
                        const mqtt5_publish_t *pub);

/** Build SUBSCRIBE packet (v5.0). Returns bytes written or negative mqtt5_err_t.
 *  @thread_safe: yes (pure function) */
int mqtt5_build_subscribe(uint8_t *buf, uint16_t size,
                          uint16_t packet_id, const char *topic,
                          uint8_t qos);

/** Build DISCONNECT packet (v5.0). Returns bytes written or negative mqtt5_err_t.
 *  @thread_safe: yes (pure function) */
int mqtt5_build_disconnect(uint8_t *buf, uint16_t size,
                           uint8_t reason_code);

/** Parse CONNACK packet (v5.0) including properties.
 *  @thread_safe: yes (pure function) */
mqtt5_err_t mqtt5_parse_connack(const uint8_t *buf, uint16_t len,
                                mqtt5_connack_t *ack);

/** Parse incoming PUBLISH packet (v5.0) including properties.
 *  @thread_safe: yes (pure function) */
mqtt5_err_t mqtt5_parse_publish(const uint8_t *buf, uint16_t len,
                                mqtt5_publish_t *pub);

/** Encode a variable byte integer (VBI) into buf. Returns bytes used (1-4)
 *  or negative mqtt5_err_t on error. @thread_safe: yes (pure function) */
int mqtt5_encode_vbi(uint8_t *buf, uint32_t value);

/** Decode a variable byte integer (VBI) from buf.
 *  @thread_safe: yes (pure function) */
mqtt5_err_t mqtt5_decode_vbi(const uint8_t *buf, uint16_t len,
                             uint32_t *value, uint8_t *bytes_used);

#ifdef __cplusplus
}
#endif

#endif /* MQTT5_PACKET_H */
