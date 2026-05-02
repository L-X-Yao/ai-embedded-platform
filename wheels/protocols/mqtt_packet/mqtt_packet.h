// SPDX-License-Identifier: MIT
/*
 * mqtt_packet — MQTT 3.1.1 packet serializer/deserializer
 *
 * Layer:   L2
 * Status:  candidate
 * Deps:      (none)
 * Instances: multi
 *
 * Stateless packet building and parsing. Handles CONNECT, PUBLISH,
 * SUBSCRIBE, PINGREQ, DISCONNECT encoding and CONNACK, PUBACK,
 * SUBACK, PINGRESP, PUBLISH decoding. No transport — caller handles
 * TCP/TLS send/receive.
 * Pure C99, no heap. All buffers caller-provided.
 */

#ifndef MQTT_PACKET_H
#define MQTT_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MQTT_PACKET_OK         =  0,
    MQTT_PACKET_ERR_PARAM  = -1,
    MQTT_PACKET_ERR_SIZE   = -2,
    MQTT_PACKET_ERR_MALFORMED = -3,
} mqtt_packet_err_t;

typedef enum {
    MQTT_CONNECT     = 1,
    MQTT_CONNACK     = 2,
    MQTT_PUBLISH     = 3,
    MQTT_PUBACK      = 4,
    MQTT_SUBSCRIBE   = 8,
    MQTT_SUBACK      = 9,
    MQTT_PINGREQ     = 12,
    MQTT_PINGRESP    = 13,
    MQTT_DISCONNECT  = 14,
} mqtt_packet_type_t;

typedef struct {
    const char *client_id;
    const char *username;
    const char *password;
    uint16_t    keepalive_s;
    bool        clean_session;
} mqtt_connect_opts_t;

typedef struct {
    bool    session_present;
    uint8_t return_code;
} mqtt_connack_t;

typedef struct {
    const char    *topic;
    const uint8_t *payload;
    size_t         payload_len;
    uint16_t       packet_id;
    uint8_t        qos;
    bool           retain;
    bool           dup;
} mqtt_publish_t;

/** @thread_safe: yes (pure function, no state) */
int mqtt_packet_build_connect(uint8_t *buf, size_t buf_size,
                              const mqtt_connect_opts_t *opts);

/** Build PUBLISH packet into buf. @thread_safe: yes (pure function) */
int mqtt_packet_build_publish(uint8_t *buf, size_t buf_size,
                              const mqtt_publish_t *pub);

/** Build SUBSCRIBE packet into buf. @thread_safe: yes (pure function) */
int mqtt_packet_build_subscribe(uint8_t *buf, size_t buf_size,
                                uint16_t packet_id,
                                const char *topic, uint8_t qos);

/** Build PINGREQ packet into buf. @thread_safe: yes (pure function) */
int mqtt_packet_build_pingreq(uint8_t *buf, size_t buf_size);

/** Build DISCONNECT packet into buf. @thread_safe: yes (pure function) */
int mqtt_packet_build_disconnect(uint8_t *buf, size_t buf_size);

/** Extract packet type from raw MQTT header byte. @thread_safe: yes (pure function) */
mqtt_packet_type_t mqtt_packet_get_type(const uint8_t *buf, size_t len);

/** Parse CONNACK packet into out struct. @thread_safe: yes (pure function) */
mqtt_packet_err_t mqtt_packet_parse_connack(const uint8_t *buf, size_t len,
                                            mqtt_connack_t *out);

/** Parse incoming PUBLISH packet fields into out struct. @thread_safe: yes (pure function) */
mqtt_packet_err_t mqtt_packet_parse_publish(const uint8_t *buf, size_t len,
                                            mqtt_publish_t *out);

/** Parse PUBACK packet and extract packet_id. @thread_safe: yes (pure function) */
mqtt_packet_err_t mqtt_packet_parse_puback(const uint8_t *buf, size_t len,
                                           uint16_t *packet_id);

/** Encode MQTT remaining-length field into buf. @thread_safe: yes (pure function) */
int mqtt_packet_encode_remaining_length(uint8_t *buf, uint32_t length);

/** Decode MQTT remaining-length field from buf. @thread_safe: yes (pure function) */
uint32_t mqtt_packet_decode_remaining_length(const uint8_t *buf, size_t len,
                                             int *bytes_used);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_PACKET_H */
