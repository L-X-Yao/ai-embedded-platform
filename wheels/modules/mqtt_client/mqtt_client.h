// SPDX-License-Identifier: MIT
/*
 * mqtt_client — stateful MQTT 3.1.1 client (feed/poll, no OS)
 *
 * Layer:     L3 (modules)
 * Status:    candidate
 * Deps:      mqtt_packet (L2)
 * Instances: multi
 *
 * Non-blocking state machine: caller feeds raw TCP bytes via feed(),
 * calls poll() periodically for keepalive/timeout, uses publish/subscribe
 * when connected. Transport injected via io function pointers.
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- configurable limits ---------- */

#ifndef MQTT_CLIENT_RX_BUF_SIZE
#define MQTT_CLIENT_RX_BUF_SIZE 1024
#endif

#ifndef MQTT_CLIENT_TX_BUF_SIZE
#define MQTT_CLIENT_TX_BUF_SIZE 512
#endif

/* ---------- error codes ---------- */

typedef enum {
    MQTT_CLIENT_OK           =  0,
    MQTT_CLIENT_ERR_PARAM    = -1,
    MQTT_CLIENT_ERR_STATE    = -2,
    MQTT_CLIENT_ERR_BUSY     = -3,
    MQTT_CLIENT_ERR_SIZE     = -4,
    MQTT_CLIENT_ERR_PROTOCOL = -5,
    MQTT_CLIENT_ERR_IO       = -6,
} mqtt_client_err_t;

/* ---------- connection state ---------- */

typedef enum {
    MQTT_CLIENT_DISCONNECTED = 0,
    MQTT_CLIENT_CONNECTING,
    MQTT_CLIENT_CONNECTED,
} mqtt_client_state_t;

/* ---------- transport injection ---------- */

typedef struct {
    int (*send)(void *ctx, const uint8_t *data, uint16_t len);
    uint32_t (*get_tick_ms)(void *ctx);
    void *ctx;
} mqtt_client_io_t;

/* ---------- configuration ---------- */

typedef struct {
    const char *client_id;
    const char *username;
    const char *password;
    uint16_t    keepalive_s;
    bool        clean_session;
    uint32_t    connect_timeout_ms;
    uint32_t    ack_timeout_ms;
} mqtt_client_cfg_t;

/* ---------- callbacks ---------- */

typedef void (*mqtt_client_msg_cb_t)(void *ctx, const char *topic,
                                     uint16_t topic_len,
                                     const uint8_t *payload, uint16_t len,
                                     uint8_t qos);

typedef void (*mqtt_client_evt_cb_t)(void *ctx, mqtt_client_state_t new_state,
                                     uint8_t reason);

/* ---------- instance ---------- */

typedef struct {
    const mqtt_client_io_t *io;       /* internal */
    mqtt_client_cfg_t cfg;            /* internal: copied at init */
    mqtt_client_state_t state;        /* internal */

    uint8_t  rx_buf[MQTT_CLIENT_RX_BUF_SIZE]; /* internal */
    uint16_t rx_len;                  /* internal */

    uint8_t  tx_buf[MQTT_CLIENT_TX_BUF_SIZE]; /* internal */

    uint32_t last_tx_ms;              /* internal */
    uint32_t last_rx_ms;              /* internal */
    uint32_t connect_start_ms;        /* internal */

    uint16_t next_packet_id;          /* internal */
    uint16_t inflight_pub_id;         /* internal: 0 = none pending */
    uint32_t inflight_pub_ms;         /* internal */
    uint16_t inflight_sub_id;         /* internal: 0 = none pending */
    uint32_t inflight_sub_ms;         /* internal */

    mqtt_client_msg_cb_t on_message;  /* internal */
    mqtt_client_evt_cb_t on_event;    /* internal */
    void *cb_ctx;                     /* internal */
} mqtt_client_t;

/* ---------- API ---------- */

/** Initialize client. @thread_safe: no @isr_safe: no */
mqtt_client_err_t mqtt_client_init(mqtt_client_t *c,
                                   const mqtt_client_io_t *io,
                                   const mqtt_client_cfg_t *cfg);

/** Send CONNECT packet and enter CONNECTING state. @thread_safe: no @isr_safe: no */
mqtt_client_err_t mqtt_client_connect(mqtt_client_t *c);

/** Send DISCONNECT and return to DISCONNECTED. @thread_safe: no @isr_safe: no */
mqtt_client_err_t mqtt_client_disconnect(mqtt_client_t *c);

/** Publish a message (QoS 0 or 1). QoS 1 allows one inflight. @thread_safe: no @isr_safe: no */
mqtt_client_err_t mqtt_client_publish(mqtt_client_t *c, const char *topic,
                                      const uint8_t *payload, uint16_t len,
                                      uint8_t qos);

/** Subscribe to a topic (QoS 0 or 1). One inflight subscribe at a time. @thread_safe: no @isr_safe: no */
mqtt_client_err_t mqtt_client_subscribe(mqtt_client_t *c,
                                        const char *topic, uint8_t qos);

/** Feed received TCP bytes. Dispatches complete MQTT packets. @thread_safe: no @isr_safe: no */
mqtt_client_err_t mqtt_client_feed(mqtt_client_t *c,
                                   const uint8_t *data, uint16_t len);

/** Poll for keepalive / timeout. Call periodically. @thread_safe: no @isr_safe: no */
mqtt_client_err_t mqtt_client_poll(mqtt_client_t *c);

/** Get current connection state. @thread_safe: no @isr_safe: yes */
mqtt_client_state_t mqtt_client_get_state(const mqtt_client_t *c);

/** Register message callback. @thread_safe: no @isr_safe: no */
void mqtt_client_on_message(mqtt_client_t *c, mqtt_client_msg_cb_t cb, void *ctx);

/** Register state-change event callback. @thread_safe: no @isr_safe: no */
void mqtt_client_on_event(mqtt_client_t *c, mqtt_client_evt_cb_t cb, void *ctx);

/** Allocate next packet ID (1-65535, wraps). @thread_safe: no @isr_safe: yes */
uint16_t mqtt_client_next_id(mqtt_client_t *c);

/** Check if QoS 1 publish is still awaiting PUBACK. @thread_safe: no @isr_safe: yes */
bool mqtt_client_pub_inflight(const mqtt_client_t *c);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_CLIENT_H */
