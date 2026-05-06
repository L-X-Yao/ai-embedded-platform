// SPDX-License-Identifier: MIT
/*
 * mqtt_client — stateful MQTT 3.1.1 client (feed/poll, no OS)
 *
 * Layer:     L3 (modules)
 * Status:    candidate
 * Deps:      mqtt_packet (L2)
 * Instances: multi
 */
#include "mqtt_client.h"
#include "mqtt_packet.h"
#include <string.h>

/* ─── helpers ────────────────────────────────────────────────── */

static uint32_t now_ms(const mqtt_client_t *c)
{
    return c->io->get_tick_ms(c->io->ctx);
}

static int send_raw(mqtt_client_t *c, const uint8_t *data, uint16_t len)
{
    int rc = c->io->send(c->io->ctx, data, len);
    if (rc == 0) c->last_tx_ms = now_ms(c);
    return rc;
}

static void set_state(mqtt_client_t *c, mqtt_client_state_t s, uint8_t reason)
{
    if (c->state == s) return;
    c->state = s;
    if (c->on_event) c->on_event(c->cb_ctx, s, reason);
}

static int try_decode_packet_len(const uint8_t *buf, uint16_t buf_len,
                                 uint32_t *remaining, uint16_t *header_bytes)
{
    if (buf_len < 2) return -1;
    uint32_t value = 0;
    uint32_t mult  = 1;
    uint16_t i;
    for (i = 1; i < buf_len && i <= 4; i++) {
        value += (uint32_t)(buf[i] & 0x7F) * mult;
        mult  *= 128;
        if (!(buf[i] & 0x80)) {
            *remaining    = value;
            *header_bytes = i + 1;
            return 0;
        }
    }
    if (i > 4) return -2;
    return -1;
}

/* ─── packet dispatch ────────────────────────────────────────── */

static void dispatch_connack(mqtt_client_t *c, const uint8_t *pkt, uint16_t len)
{
    if (c->state != MQTT_CLIENT_CONNECTING) return;
    mqtt_connack_t ack;
    if (mqtt_packet_parse_connack(pkt, len, &ack) != MQTT_PACKET_OK) {
        set_state(c, MQTT_CLIENT_DISCONNECTED, 0xFF);
        return;
    }
    if (ack.return_code == 0) {
        set_state(c, MQTT_CLIENT_CONNECTED, 0);
    } else {
        set_state(c, MQTT_CLIENT_DISCONNECTED, ack.return_code);
    }
}

static void dispatch_publish(mqtt_client_t *c, const uint8_t *pkt, uint16_t len)
{
    mqtt_publish_t pub;
    if (mqtt_packet_parse_publish(pkt, len, &pub) != MQTT_PACKET_OK) return;

    if (c->on_message) {
        c->on_message(c->cb_ctx, pub.topic,
                      pub.payload, (uint16_t)pub.payload_len, pub.qos);
    }

    if (pub.qos == 1 && pub.packet_id != 0) {
        uint8_t ack[4] = { (MQTT_PUBACK << 4), 2,
                           (uint8_t)(pub.packet_id >> 8),
                           (uint8_t)(pub.packet_id & 0xFF) };
        send_raw(c, ack, 4);
    }
}

static void dispatch_puback(mqtt_client_t *c, const uint8_t *pkt, uint16_t len)
{
    uint16_t id;
    if (mqtt_packet_parse_puback(pkt, len, &id) != MQTT_PACKET_OK) return;
    if (c->inflight_pub_id == id) c->inflight_pub_id = 0;
}

static void dispatch_suback(mqtt_client_t *c, const uint8_t *pkt, uint16_t len)
{
    if (len < 5) return;
    uint16_t id = (uint16_t)((pkt[2] << 8) | pkt[3]);
    if (c->inflight_sub_id == id) c->inflight_sub_id = 0;
    (void)pkt[4]; /* return code — caller can check via inflight_sub_id cleared */
}

static void dispatch_packet(mqtt_client_t *c, const uint8_t *pkt, uint16_t len)
{
    if (len == 0) return;
    c->last_rx_ms = now_ms(c);

    mqtt_packet_type_t t = mqtt_packet_get_type(pkt, len);
    switch (t) {
    case MQTT_CONNACK:  dispatch_connack(c, pkt, len); break;
    case MQTT_PUBLISH:  dispatch_publish(c, pkt, len); break;
    case MQTT_PUBACK:   dispatch_puback(c, pkt, len);  break;
    case MQTT_SUBACK:   dispatch_suback(c, pkt, len);  break;
    case MQTT_PINGRESP: break; /* last_rx_ms already updated */
    default: break;
    }
}

/* ─── public API ─────────────────────────────────────────────── */

mqtt_client_err_t mqtt_client_init(mqtt_client_t *c,
                                   const mqtt_client_io_t *io,
                                   const mqtt_client_cfg_t *cfg)
{
    if (!c || !io || !io->send || !io->get_tick_ms || !cfg) return MQTT_CLIENT_ERR_PARAM;
    if (!cfg->client_id) return MQTT_CLIENT_ERR_PARAM;

    memset(c, 0, sizeof(*c));
    c->io    = io;
    c->cfg   = *cfg;
    c->state = MQTT_CLIENT_DISCONNECTED;
    c->next_packet_id = 1;

    if (c->cfg.connect_timeout_ms == 0) c->cfg.connect_timeout_ms = 10000;
    if (c->cfg.ack_timeout_ms == 0)     c->cfg.ack_timeout_ms = 10000;

    return MQTT_CLIENT_OK;
}

mqtt_client_err_t mqtt_client_connect(mqtt_client_t *c)
{
    if (!c) return MQTT_CLIENT_ERR_PARAM;
    if (c->state != MQTT_CLIENT_DISCONNECTED) return MQTT_CLIENT_ERR_STATE;

    mqtt_connect_opts_t opts = {
        .client_id     = c->cfg.client_id,
        .username      = c->cfg.username,
        .password      = c->cfg.password,
        .keepalive_s   = c->cfg.keepalive_s,
        .clean_session = c->cfg.clean_session,
    };
    int len = mqtt_packet_build_connect(c->tx_buf, sizeof(c->tx_buf), &opts);
    if (len <= 0) return MQTT_CLIENT_ERR_SIZE;

    if (send_raw(c, c->tx_buf, (uint16_t)len) != 0) return MQTT_CLIENT_ERR_IO;

    c->rx_len           = 0;
    c->inflight_pub_id  = 0;
    c->inflight_sub_id  = 0;
    c->connect_start_ms = now_ms(c);
    c->last_rx_ms       = c->connect_start_ms;
    set_state(c, MQTT_CLIENT_CONNECTING, 0);
    return MQTT_CLIENT_OK;
}

mqtt_client_err_t mqtt_client_disconnect(mqtt_client_t *c)
{
    if (!c) return MQTT_CLIENT_ERR_PARAM;
    if (c->state != MQTT_CLIENT_CONNECTED) return MQTT_CLIENT_ERR_STATE;

    int len = mqtt_packet_build_disconnect(c->tx_buf, sizeof(c->tx_buf));
    if (len > 0) send_raw(c, c->tx_buf, (uint16_t)len);

    set_state(c, MQTT_CLIENT_DISCONNECTED, 0);
    return MQTT_CLIENT_OK;
}

mqtt_client_err_t mqtt_client_publish(mqtt_client_t *c, const char *topic,
                                      const uint8_t *payload, uint16_t len,
                                      uint8_t qos)
{
    if (!c || !topic) return MQTT_CLIENT_ERR_PARAM;
    if (c->state != MQTT_CLIENT_CONNECTED) return MQTT_CLIENT_ERR_STATE;
    if (qos > 1) return MQTT_CLIENT_ERR_PARAM;
    if (qos == 1 && c->inflight_pub_id != 0) return MQTT_CLIENT_ERR_BUSY;

    uint16_t pkt_id = (qos == 1) ? mqtt_client_next_id(c) : 0;

    mqtt_publish_t pub = {
        .topic       = topic,
        .payload     = payload,
        .payload_len = len,
        .packet_id   = pkt_id,
        .qos         = qos,
        .retain      = false,
        .dup         = false,
    };
    int pkt_len = mqtt_packet_build_publish(c->tx_buf, sizeof(c->tx_buf), &pub);
    if (pkt_len <= 0) return MQTT_CLIENT_ERR_SIZE;

    if (send_raw(c, c->tx_buf, (uint16_t)pkt_len) != 0) return MQTT_CLIENT_ERR_IO;

    if (qos == 1) {
        c->inflight_pub_id = pkt_id;
        c->inflight_pub_ms = now_ms(c);
    }
    return MQTT_CLIENT_OK;
}

mqtt_client_err_t mqtt_client_subscribe(mqtt_client_t *c,
                                        const char *topic, uint8_t qos)
{
    if (!c || !topic) return MQTT_CLIENT_ERR_PARAM;
    if (c->state != MQTT_CLIENT_CONNECTED) return MQTT_CLIENT_ERR_STATE;
    if (c->inflight_sub_id != 0) return MQTT_CLIENT_ERR_BUSY;

    uint16_t pkt_id = mqtt_client_next_id(c);
    int pkt_len = mqtt_packet_build_subscribe(c->tx_buf, sizeof(c->tx_buf),
                                              pkt_id, topic, qos);
    if (pkt_len <= 0) return MQTT_CLIENT_ERR_SIZE;

    if (send_raw(c, c->tx_buf, (uint16_t)pkt_len) != 0) return MQTT_CLIENT_ERR_IO;

    c->inflight_sub_id = pkt_id;
    c->inflight_sub_ms = now_ms(c);
    return MQTT_CLIENT_OK;
}

mqtt_client_err_t mqtt_client_feed(mqtt_client_t *c,
                                   const uint8_t *data, uint16_t len)
{
    if (!c || !data) return MQTT_CLIENT_ERR_PARAM;
    if (c->state == MQTT_CLIENT_DISCONNECTED) return MQTT_CLIENT_ERR_STATE;

    uint16_t to_copy = len;
    if (c->rx_len + to_copy > MQTT_CLIENT_RX_BUF_SIZE) {
        to_copy = (uint16_t)(MQTT_CLIENT_RX_BUF_SIZE - c->rx_len);
        if (to_copy == 0) {
            c->rx_len = 0;
            return MQTT_CLIENT_ERR_SIZE;
        }
    }
    memcpy(&c->rx_buf[c->rx_len], data, to_copy);
    c->rx_len += to_copy;

    for (;;) {
        if (c->rx_len < 2) break;

        uint32_t remaining;
        uint16_t hdr_bytes;
        int rc = try_decode_packet_len(c->rx_buf, c->rx_len, &remaining, &hdr_bytes);
        if (rc == -1) break;
        if (rc == -2) { c->rx_len = 0; return MQTT_CLIENT_ERR_PROTOCOL; }

        uint32_t total = (uint32_t)hdr_bytes + remaining;
        if (total > MQTT_CLIENT_RX_BUF_SIZE) { c->rx_len = 0; return MQTT_CLIENT_ERR_SIZE; }
        if (c->rx_len < (uint16_t)total) break;

        dispatch_packet(c, c->rx_buf, (uint16_t)total);

        uint16_t consumed = (uint16_t)total;
        if (c->rx_len > consumed) {
            memmove(c->rx_buf, &c->rx_buf[consumed], c->rx_len - consumed);
        }
        c->rx_len -= consumed;
    }

    return MQTT_CLIENT_OK;
}

mqtt_client_err_t mqtt_client_poll(mqtt_client_t *c)
{
    if (!c) return MQTT_CLIENT_ERR_PARAM;

    uint32_t t = now_ms(c);

    if (c->state == MQTT_CLIENT_CONNECTING) {
        if ((t - c->connect_start_ms) >= c->cfg.connect_timeout_ms) {
            set_state(c, MQTT_CLIENT_DISCONNECTED, 0xFF);
        }
        return MQTT_CLIENT_OK;
    }

    if (c->state != MQTT_CLIENT_CONNECTED) return MQTT_CLIENT_OK;

    /* keepalive */
    if (c->cfg.keepalive_s > 0) {
        uint32_t ka_ms = (uint32_t)c->cfg.keepalive_s * 1000;

        if ((t - c->last_tx_ms) >= (ka_ms * 3 / 4)) {
            int len = mqtt_packet_build_pingreq(c->tx_buf, sizeof(c->tx_buf));
            if (len > 0) send_raw(c, c->tx_buf, (uint16_t)len);
        }

        if ((t - c->last_rx_ms) >= (ka_ms * 3 / 2)) {
            set_state(c, MQTT_CLIENT_DISCONNECTED, 0xFE);
            return MQTT_CLIENT_OK;
        }
    }

    /* PUBACK timeout */
    if (c->inflight_pub_id != 0 &&
        (t - c->inflight_pub_ms) >= c->cfg.ack_timeout_ms) {
        c->inflight_pub_id = 0;
    }

    /* SUBACK timeout */
    if (c->inflight_sub_id != 0 &&
        (t - c->inflight_sub_ms) >= c->cfg.ack_timeout_ms) {
        c->inflight_sub_id = 0;
    }

    return MQTT_CLIENT_OK;
}

mqtt_client_state_t mqtt_client_get_state(const mqtt_client_t *c)
{
    if (!c) return MQTT_CLIENT_DISCONNECTED;
    return c->state;
}

void mqtt_client_on_message(mqtt_client_t *c, mqtt_client_msg_cb_t cb, void *ctx)
{
    if (!c) return;
    c->on_message = cb;
    c->cb_ctx     = ctx;
}

void mqtt_client_on_event(mqtt_client_t *c, mqtt_client_evt_cb_t cb, void *ctx)
{
    if (!c) return;
    c->on_event = cb;
    c->cb_ctx   = ctx;
}

uint16_t mqtt_client_next_id(mqtt_client_t *c)
{
    if (!c) return 0;
    uint16_t id = c->next_packet_id;
    c->next_packet_id = (id >= 0xFFFF) ? 1 : (uint16_t)(id + 1);
    return id;
}

bool mqtt_client_pub_inflight(const mqtt_client_t *c)
{
    if (!c) return false;
    return c->inflight_pub_id != 0;
}
