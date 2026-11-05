// SPDX-License-Identifier: MIT
// LibFuzzer harness for mqtt5_packet — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_mqtt5_packet.c ../mqtt5_packet.c -o fuzz_mqtt5_packet
// Run:     ./fuzz_mqtt5_packet -max_total_time=60 corpus_mqtt5_packet/

#include "../mqtt5_packet.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define STR_MAX 64
#define PAYLOAD_MAX 300

// Variable byte integers stay under two bytes for these bodies.
static size_t vbi_field(size_t value) {
    return value < 128 ? 1 : 2;
}

// The fixed header must announce exactly the bytes that follow it.
static void check_announced(const uint8_t *wire, size_t need, size_t body) {
    uint32_t announced = 0;
    uint8_t used = 0;
    if (mqtt5_decode_vbi(wire + 1, (uint16_t)(need - 1), &announced, &used) != MQTT5_OK ||
        announced != body || 1 + (size_t)used + body != need) {
        __builtin_trap();
    }
}

// Check a length-prefixed string and return what follows it.
static const uint8_t *next_string(const uint8_t *p, const char *want) {
    size_t len = strlen(want);
    if (p[0] != (uint8_t)(len >> 8) || p[1] != (uint8_t)len ||
        memcmp(p + 2, want, len) != 0) {
        __builtin_trap();
    }
    return p + 2 + len;
}

// This wheel has no CONNECT parser, so read the frame here.
static void check_connect(const mqtt5_connect_t *cfg, const uint8_t *wire,
                          size_t need, size_t body) {
    static const uint8_t protocol[] = {0, 4, 'M', 'Q', 'T', 'T', 5};
    uint8_t flags = (uint8_t)((cfg->clean_start ? 0x02 : 0) | (cfg->username ? 0x80 : 0) |
                              (cfg->password ? 0x40 : 0));
    uint8_t props = cfg->session_expiry > 0 ? 5 : 0;
    const uint8_t *p = wire + 1 + vbi_field(body);

    check_announced(wire, need, body);
    if (wire[0] != 0x10 || memcmp(p, protocol, sizeof(protocol)) != 0 || p[7] != flags ||
        p[8] != (uint8_t)(cfg->keep_alive >> 8) || p[9] != (uint8_t)cfg->keep_alive ||
        p[10] != props) {
        __builtin_trap();
    }
    p += 11;
    if (props > 0) {
        uint32_t expiry = ((uint32_t)p[1] << 24) | ((uint32_t)p[2] << 16) |
                          ((uint32_t)p[3] << 8) | p[4];
        if (p[0] != MQTT5_PROP_SESSION_EXPIRY || expiry != cfg->session_expiry) __builtin_trap();
        p += props;
    }
    p = next_string(p, cfg->client_id);
    if (cfg->username) p = next_string(p, cfg->username);
    if (cfg->password) p = next_string(p, cfg->password);
    if (p != wire + need) __builtin_trap();
}

static void check_subscribe(const uint8_t *wire, size_t need, size_t body,
                            uint16_t id, const char *topic, uint8_t qos) {
    const uint8_t *p = wire + 1 + vbi_field(body);

    check_announced(wire, need, body);
    if (wire[0] != 0x82 || p[0] != (uint8_t)(id >> 8) || p[1] != (uint8_t)id || p[2] != 0) {
        __builtin_trap();
    }
    p = next_string(p + 3, topic);
    if (p[0] != qos || p + 1 != wire + need) __builtin_trap();
}

static void check_vbi(uint32_t value) {
    uint8_t *buf = malloc(4);
    if (!buf) return;

    int n = mqtt5_encode_vbi(buf, value);
    if (value > 268435455u) {
        if (n >= 0) __builtin_trap();
    } else {
        uint32_t back = 0;
        uint8_t used = 0;
        if (n < 1 || n > 4) __builtin_trap();
        if (mqtt5_decode_vbi(buf, (uint16_t)n, &back, &used) != MQTT5_OK ||
            back != value || used != n) {
            __builtin_trap();
        }
    }
    free(buf);
}

static void check_publish(const mqtt5_publish_t *pub, size_t topic_len,
                          const uint8_t *wire, int len) {
    mqtt5_publish_t back;
    memset(&back, 0, sizeof(back));
    if (mqtt5_parse_publish(wire, (uint16_t)len, &back) != MQTT5_OK ||
        back.qos != pub->qos || back.retain != pub->retain ||
        back.message_expiry != pub->message_expiry ||
        memcmp(back.topic, pub->topic, topic_len) != 0 ||
        back.payload_len != pub->payload_len ||
        (pub->payload_len > 0 && memcmp(back.payload, pub->payload, pub->payload_len) != 0)) {
        __builtin_trap();
    }
    if (pub->qos > 0 && back.packet_id != pub->packet_id) __builtin_trap();
}

static void round_trip_publish(fuzz_input_t *in) {
    char topic[STR_MAX + 1];
    uint8_t payload[PAYLOAD_MAX];
    uint8_t op = fuzz_u8(in);
    size_t topic_want = fuzz_u8(in) % (STR_MAX + 1);
    size_t topic_len = fuzz_take_str(in, topic, topic_want);
    size_t payload_want = fuzz_u16(in) % (PAYLOAD_MAX + 1);

    mqtt5_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.topic = topic;
    pub.payload_len = (uint16_t)fuzz_take(in, payload, payload_want);
    pub.payload = pub.payload_len > 0 ? payload : NULL;
    pub.packet_id = fuzz_u16(in);
    pub.qos = op & 0x03;
    pub.retain = (op & 0x04) != 0;
    if (op & 0x08) {
        uint32_t hi = fuzz_u16(in);
        pub.message_expiry = (hi << 16) | fuzz_u16(in);
    }

    // Exact-size frame, so ASan sees any write past the end.
    size_t props = pub.message_expiry > 0 ? 5 : 0;
    size_t body = 2 + topic_len + (pub.qos > 0 ? 2 : 0) + 1 + props + pub.payload_len;
    size_t need = 1 + vbi_field(body) + body;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    if (mqtt5_build_publish(wire, (uint16_t)(need - 1), &pub) >= 0) __builtin_trap();

    // A length with no bytes would announce a payload the frame does not hold.
    if (pub.payload_len > 0) {
        pub.payload = NULL;
        if (mqtt5_build_publish(wire, (uint16_t)need, &pub) >= 0) __builtin_trap();
        pub.payload = payload;
    }

    int len = mqtt5_build_publish(wire, (uint16_t)need, &pub);
    if (pub.qos > 2) {
        if (len >= 0) __builtin_trap();
    } else {
        if (len != (int)need) __builtin_trap();
        check_announced(wire, need, body);
        check_publish(&pub, topic_len, wire, len);
    }
    free(wire);
}

static void round_trip_connect(fuzz_input_t *in) {
    char client[STR_MAX + 1], user[STR_MAX + 1], pass[STR_MAX + 1];
    uint8_t op = fuzz_u8(in);
    size_t want = fuzz_u8(in) % (STR_MAX + 1);
    size_t payload = 2 + fuzz_take_str(in, client, want);

    mqtt5_connect_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.client_id = client;
    cfg.keep_alive = fuzz_u16(in);
    cfg.clean_start = (op & 0x01) != 0;
    if (op & 0x02) {
        want = fuzz_u8(in) % (STR_MAX + 1);
        payload += 2 + fuzz_take_str(in, user, want);
        cfg.username = user;
    }
    if (op & 0x04) {
        want = fuzz_u8(in) % (STR_MAX + 1);
        payload += 2 + fuzz_take_str(in, pass, want);
        cfg.password = pass;
    }
    if (op & 0x08) {
        uint32_t hi = fuzz_u16(in);
        cfg.session_expiry = (hi << 16) | fuzz_u16(in);
    }

    size_t body = 10 + 1 + (cfg.session_expiry > 0 ? 5 : 0) + payload;
    size_t need = 1 + vbi_field(body) + body;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    if (mqtt5_build_connect(wire, (uint16_t)(need - 1), &cfg) >= 0) __builtin_trap();

    int len = mqtt5_build_connect(wire, (uint16_t)need, &cfg);
    if (cfg.password && !cfg.username) {
        if (len >= 0) __builtin_trap();
    } else {
        if (len != (int)need) __builtin_trap();
        check_connect(&cfg, wire, need, body);
    }
    free(wire);
}

// The buffer size is 16 bits, so the largest frame stops at 65535 bytes.
static void check_largest_publish(uint8_t extra) {
    static char text[UINT16_MAX + 1];
    size_t len = UINT16_MAX - 15 + extra % 16;
    memset(text, 't', len);
    text[len] = 0;

    size_t need = 1 + 3 + 2 + len + 1;
    size_t room = need > UINT16_MAX ? UINT16_MAX : need;
    uint8_t *wire = malloc(room);
    if (!wire) return;

    mqtt5_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.topic = text;

    int n = mqtt5_build_publish(wire, (uint16_t)room, &pub);
    if (need > UINT16_MAX ? n >= 0 : n != (int)need) __builtin_trap();
    free(wire);
}

static void check_subscribe_and_disconnect(fuzz_input_t *in) {
    char topic[STR_MAX + 1];
    uint8_t qos = fuzz_u8(in) % 5;
    uint16_t id = fuzz_u16(in);
    size_t want = fuzz_u8(in) % (STR_MAX + 1);
    size_t body = 2 + 1 + 2 + fuzz_take_str(in, topic, want) + 1;
    size_t need = 1 + vbi_field(body) + body;

    uint8_t *wire = malloc(need);
    if (!wire) return;

    if (mqtt5_build_subscribe(wire, (uint16_t)(need - 1), id, topic, qos) >= 0) {
        __builtin_trap();
    }
    int len = mqtt5_build_subscribe(wire, (uint16_t)need, id, topic, qos);
    if ((qos > 2) != (len < 0)) __builtin_trap();
    if (len >= 0) {
        if (len != (int)need) __builtin_trap();
        check_subscribe(wire, need, body, id, topic, qos);
    }

    free(wire);

    // Reason code 0 has a two-byte short form; any other code takes four bytes.
    uint8_t reason = fuzz_u8(in);
    size_t disconnect = reason == 0 ? 2 : 4;
    wire = malloc(disconnect);
    if (!wire) return;
    if (mqtt5_build_disconnect(wire, (uint16_t)(disconnect - 1), reason) >= 0) __builtin_trap();
    if (mqtt5_build_disconnect(wire, (uint16_t)disconnect, reason) != (int)disconnect ||
        wire[0] != 0xE0 || wire[1] != disconnect - 2) {
        __builtin_trap();
    }
    if (reason != 0 && (wire[2] != reason || wire[3] != 0)) __builtin_trap();
    free(wire);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint16_t len = (uint16_t)(size > 65535 ? 65535 : size);

    // Parse CONNACK (v5.0)
    mqtt5_connack_t connack;
    memset(&connack, 0, sizeof(connack));
    mqtt5_parse_connack(data, len, &connack);

    // Parse PUBLISH (v5.0)
    mqtt5_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    if (mqtt5_parse_publish(data, len, &pub) == MQTT5_OK && pub.qos > 2) __builtin_trap();

    // Decode variable byte integer
    uint32_t value;
    uint8_t bytes_used;
    mqtt5_decode_vbi(data, len, &value, &bytes_used);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    check_largest_publish(size > 0 ? data[0] : 0);
    while (in.left > 0) {
        uint32_t hi = fuzz_u16(&in);
        check_vbi((hi << 16) | fuzz_u16(&in));
        round_trip_publish(&in);
        round_trip_connect(&in);
        check_subscribe_and_disconnect(&in);
    }

    return 0;
}
