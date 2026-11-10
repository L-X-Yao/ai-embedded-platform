// SPDX-License-Identifier: MIT
// LibFuzzer harness for mqtt_client -- coverage-guided fuzzing
//
// Runs connect, broker bytes, publish, subscribe, poll and disconnect in the
// order the input picks, then feeds one stream to two clients, whole and byte
// by byte. How TCP splits the stream must not change what the client does.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../protocols/mqtt_packet \
//          -I../../../../tooling/harness fuzz_libfuzzer_mqtt_client.c ../mqtt_client.c \
//          ../../../protocols/mqtt_packet/mqtt_packet.c -o fuzz_mqtt_client
// Run:     ./fuzz_mqtt_client -max_total_time=60 corpus_mqtt_client/

#include "../mqtt_client.h"
#include "mqtt_packet.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define TOPIC_CAP 96
#define BODY_CAP (MQTT_CLIENT_TX_BUF_SIZE + 32)
#define STREAM_CAP (MQTT_CLIENT_RX_BUF_SIZE * 3)

typedef struct {
    const mqtt_client_t *client;
    uint32_t now;
    int      send_rc;
    uint8_t  last[MQTT_CLIENT_TX_BUF_SIZE];
    uint16_t last_len;
    uint32_t sends;
    uint32_t sent_hash;
    uint32_t messages;
    uint32_t message_hash;
} world_t;

static uint32_t mix(uint32_t hash, const void *data, size_t len) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < len; i++) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static int world_send(void *ctx, const uint8_t *data, uint16_t len) {
    world_t *w = ctx;
    if (len == 0 || len > sizeof(w->last)) __builtin_trap();
    memcpy(w->last, data, len);
    w->last_len = len;
    w->sends++;
    w->sent_hash = mix(w->sent_hash, data, len);
    return w->send_rc;
}

static uint32_t world_tick(void *ctx) {
    return ((world_t *)ctx)->now;
}

// Topic and payload point into the client's receive buffer, past which ASan
// sees nothing, so stay inside it here.
static void on_message(void *ctx, const char *topic, uint16_t topic_len,
                       const uint8_t *payload, uint16_t len, uint8_t qos) {
    world_t *w = ctx;
    const uint8_t *low = w->client->rx_buf;
    const uint8_t *high = low + sizeof(w->client->rx_buf);
    const uint8_t *name = (const uint8_t *)topic;

    if (name < low || name + topic_len > high) __builtin_trap();
    if (len > 0 && (payload < low || payload + len > high)) __builtin_trap();
    if (qos > 2) __builtin_trap();

    w->messages++;
    w->message_hash = mix(w->message_hash, topic, topic_len);
    w->message_hash = mix(w->message_hash, &qos, 1);
    if (len > 0) w->message_hash = mix(w->message_hash, payload, len);
}

static void start(mqtt_client_t *c, mqtt_client_io_t *io, world_t *w, uint16_t keepalive) {
    memset(w, 0, sizeof(*w));
    w->client = c;
    io->send = world_send;
    io->get_tick_ms = world_tick;
    io->ctx = w;

    mqtt_client_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.client_id = "fuzz";
    cfg.keepalive_s = keepalive;
    cfg.clean_session = true;
    if (mqtt_client_init(c, io, &cfg) != MQTT_CLIENT_OK) __builtin_trap();
    mqtt_client_on_message(c, on_message, w);
}

static void check_invariants(const mqtt_client_t *c) {
    if (c->rx_len > sizeof(c->rx_buf)) __builtin_trap();
    if (c->state != MQTT_CLIENT_DISCONNECTED && c->state != MQTT_CLIENT_CONNECTING &&
        c->state != MQTT_CLIENT_CONNECTED) {
        __builtin_trap();
    }
}

static void op_publish(mqtt_client_t *c, world_t *w, fuzz_input_t *in) {
    static uint8_t body[BODY_CAP];
    char topic[TOPIC_CAP + 1];
    fuzz_take_str(in, topic, 1 + fuzz_u8(in) % TOPIC_CAP);
    size_t len = fuzz_take(in, body, fuzz_u16(in) % sizeof(body));
    uint8_t qos = fuzz_u8(in) % 4;
    mqtt_client_state_t before = c->state;
    bool busy = qos == 1 && c->inflight_pub_id != 0;
    uint32_t sends = w->sends;

    mqtt_client_err_t err = mqtt_client_publish(c, topic, body, (uint16_t)len, qos);
    if (before != MQTT_CLIENT_CONNECTED) {
        if (err != MQTT_CLIENT_ERR_STATE) __builtin_trap();
    } else if (qos > 1) {
        if (err != MQTT_CLIENT_ERR_PARAM) __builtin_trap();
    } else if (busy) {
        if (err != MQTT_CLIENT_ERR_BUSY) __builtin_trap();
    }
    if (err != MQTT_CLIENT_OK) return;

    mqtt_publish_t sent;
    if (w->sends != sends + 1) __builtin_trap();
    if (mqtt_packet_parse_publish(w->last, w->last_len, &sent) != MQTT_PACKET_OK ||
        sent.qos != qos || sent.payload_len != len || sent.topic_len != strlen(topic) ||
        memcmp(sent.topic, topic, sent.topic_len) != 0 ||
        (len > 0 && memcmp(sent.payload, body, len) != 0)) {
        __builtin_trap();
    }
    if (qos == 1 && !mqtt_client_pub_inflight(c)) __builtin_trap();
}

static void op_subscribe(mqtt_client_t *c, world_t *w, fuzz_input_t *in) {
    char topic[TOPIC_CAP + 1];
    fuzz_take_str(in, topic, 1 + fuzz_u8(in) % TOPIC_CAP);
    uint8_t qos = fuzz_u8(in) % 4;
    mqtt_client_state_t before = c->state;

    mqtt_client_err_t err = mqtt_client_subscribe(c, topic, qos);
    if (before != MQTT_CLIENT_CONNECTED) {
        if (err != MQTT_CLIENT_ERR_STATE) __builtin_trap();
    } else if (qos > 1) {
        if (err != MQTT_CLIENT_ERR_PARAM) __builtin_trap();
    }
    if (err == MQTT_CLIENT_OK &&
        (c->inflight_sub_id == 0 ||
         mqtt_packet_get_type(w->last, w->last_len) != MQTT_SUBSCRIBE)) {
        __builtin_trap();
    }
}

// A frame the broker would send, so the session gets past raw noise.
static void op_broker_frame(mqtt_client_t *c, fuzz_input_t *in) {
    static uint8_t body[MQTT_CLIENT_RX_BUF_SIZE];
    uint8_t frame[MQTT_CLIENT_RX_BUF_SIZE];
    char topic[TOPIC_CAP + 1];
    mqtt_publish_t pub;
    int len = 0;

    switch (fuzz_u8(in) % 4) {
    case 0:
        frame[0] = 0x20; frame[1] = 2; frame[2] = 0;
        frame[3] = fuzz_u8(in) % 8 == 0 ? 5 : 0;
        len = 4;
        break;
    case 1:
        frame[0] = 0x40; frame[1] = 2;
        frame[2] = (uint8_t)(c->inflight_pub_id >> 8);
        frame[3] = (uint8_t)c->inflight_pub_id;
        len = 4;
        break;
    case 2:
        frame[0] = 0x90; frame[1] = 3;
        frame[2] = (uint8_t)(c->inflight_sub_id >> 8);
        frame[3] = (uint8_t)c->inflight_sub_id;
        frame[4] = fuzz_u8(in);
        len = 5;
        break;
    default:
        fuzz_take_str(in, topic, 1 + fuzz_u8(in) % TOPIC_CAP);
        memset(&pub, 0, sizeof(pub));
        pub.topic = topic;
        pub.payload = body;
        pub.payload_len = fuzz_take(in, body, fuzz_u16(in) % 300);
        pub.qos = fuzz_u8(in) % 2;
        pub.packet_id = pub.qos ? (uint16_t)(1 + fuzz_u8(in)) : 0;
        len = mqtt_packet_build_publish(frame, sizeof(frame), &pub);
        break;
    }
    if (len > 0) mqtt_client_feed(c, frame, (uint16_t)len);
}

static void run_session(fuzz_input_t *in) {
    static uint8_t bytes[STREAM_CAP];
    mqtt_client_t *c = malloc(sizeof(*c));
    world_t *w = malloc(sizeof(*w));
    mqtt_client_io_t io;
    if (c && w) {
        start(c, &io, w, fuzz_u16(in) % 600);
        for (int step = 0; step < 64 && in->left > 0; step++) {
            w->send_rc = fuzz_u8(in) % 16 == 0 ? -1 : 0;
            switch (fuzz_u8(in) % 7) {
            case 0: {
                mqtt_client_state_t before = c->state;
                mqtt_client_err_t err = mqtt_client_connect(c);
                if (before != MQTT_CLIENT_DISCONNECTED && err != MQTT_CLIENT_ERR_STATE) __builtin_trap();
                if (err == MQTT_CLIENT_OK && c->state != MQTT_CLIENT_CONNECTING) __builtin_trap();
                break;
            }
            case 1:
                mqtt_client_feed(c, bytes, (uint16_t)fuzz_take(in, bytes, fuzz_u16(in) % sizeof(bytes)));
                break;
            case 2: op_broker_frame(c, in); break;
            case 3: op_publish(c, w, in); break;
            case 4: op_subscribe(c, w, in); break;
            case 5: {
                w->now += fuzz_u16(in) * 32u;
                bool pending = mqtt_client_pub_inflight(c);
                mqtt_client_err_t err = mqtt_client_poll(c);
                // Only a publish that was waiting can time out, and it stops waiting.
                if (err == MQTT_CLIENT_ERR_TIMEOUT &&
                    (!pending || mqtt_client_pub_inflight(c))) __builtin_trap();
                break;
            }
            default:
                mqtt_client_disconnect(c);
                break;
            }
            check_invariants(c);
        }
    }
    free(c);
    free(w);
}

// Feed in pieces the input picks. When it runs out, the rest goes in one piece.
static void feed_in_pieces(mqtt_client_t *c, const uint8_t *stream, size_t len,
                           fuzz_input_t *in) {
    size_t pos = 0;
    while (pos < len) {
        size_t left = len - pos;
        size_t n = fuzz_u16(in);
        if (n == 0 || n > left) n = left;
        mqtt_client_feed(c, stream + pos, (uint16_t)n);
        check_invariants(c);
        pos += n;
    }
}

static void run_stream(fuzz_input_t *in) {
    static uint8_t stream[STREAM_CAP];
    static const uint8_t connack[] = { 0x20, 2, 0, 0 };
    mqtt_client_t *a = malloc(sizeof(*a));
    mqtt_client_t *b = malloc(sizeof(*b));
    world_t *wa = malloc(sizeof(*wa));
    world_t *wb = malloc(sizeof(*wb));
    mqtt_client_io_t ioa;
    mqtt_client_io_t iob;

    if (a && b && wa && wb) {
        start(a, &ioa, wa, 0);
        start(b, &iob, wb, 0);
        if (mqtt_client_connect(a) != MQTT_CLIENT_OK) __builtin_trap();
        if (mqtt_client_connect(b) != MQTT_CLIENT_OK) __builtin_trap();

        memcpy(stream, connack, sizeof(connack));
        size_t want = fuzz_u16(in) % (sizeof(stream) - sizeof(connack));
        size_t len = sizeof(connack) + fuzz_take(in, stream + sizeof(connack), want);

        feed_in_pieces(a, stream, len, in);
        for (size_t i = 0; i < len; i++) {
            mqtt_client_feed(b, stream + i, 1);
            check_invariants(b);
        }

        if (a->state != b->state || wa->messages != wb->messages ||
            wa->message_hash != wb->message_hash || wa->sends != wb->sends ||
            wa->sent_hash != wb->sent_hash) {
            __builtin_trap();
        }
    }
    free(a);
    free(b);
    free(wa);
    free(wb);
}

// A packet too big to buffer is dropped whole, and the next one still arrives.
static void run_oversized(fuzz_input_t *in) {
    static uint8_t stream[STREAM_CAP];
    static const uint8_t connack[] = { 0x20, 2, 0, 0 };
    static const uint8_t next[] = { 0x30, 4, 0, 1, 'k', 'v' };
    mqtt_client_t *c = malloc(sizeof(*c));
    world_t *w = malloc(sizeof(*w));
    mqtt_client_io_t io;

    if (c && w) {
        start(c, &io, w, 0);
        if (mqtt_client_connect(c) != MQTT_CLIENT_OK) __builtin_trap();
        if (mqtt_client_feed(c, connack, sizeof(connack)) != MQTT_CLIENT_OK) __builtin_trap();

        size_t total = MQTT_CLIENT_RX_BUF_SIZE + 1 + fuzz_u16(in) % MQTT_CLIENT_RX_BUF_SIZE;
        size_t remaining = total - 3;
        stream[0] = 0x30;
        stream[1] = (uint8_t)(0x80 | (remaining & 0x7F));
        stream[2] = (uint8_t)(remaining >> 7);
        memset(stream + 3, fuzz_u8(in), remaining);
        memcpy(stream + total, next, sizeof(next));

        feed_in_pieces(c, stream, total + sizeof(next), in);
        if (w->messages != 1 || c->state != MQTT_CLIENT_CONNECTED) __builtin_trap();
    }
    free(c);
    free(w);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_input_t in;

    fuzz_input_init(&in, data, size);
    run_session(&in);

    fuzz_input_init(&in, data, size);
    run_stream(&in);

    fuzz_input_init(&in, data, size);
    run_oversized(&in);

    return 0;
}
