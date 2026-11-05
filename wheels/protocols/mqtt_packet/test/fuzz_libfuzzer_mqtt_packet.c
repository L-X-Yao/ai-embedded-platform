// SPDX-License-Identifier: MIT
// LibFuzzer harness for mqtt_packet — coverage-guided fuzzing
//
// Parses the raw input, then builds frames from it in exact-size buffers, so
// ASan sees any write past the end, and reads them back.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_mqtt_packet.c ../mqtt_packet.c -o fuzz_mqtt_packet
// Run:     ./fuzz_mqtt_packet -max_total_time=60 corpus_mqtt_packet/

#include "../mqtt_packet.h"
#include "fuzz_input.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define STR_MAX 64
#define PAYLOAD_MAX 300

// Remaining length takes 1 to 4 bytes; these bodies stay under 2 of them.
static size_t remaining_field(size_t body) {
    return body < 128 ? 1 : 2;
}

// The fixed header must announce exactly the bytes that follow it.
static void check_announced(const uint8_t *wire, size_t need, size_t body) {
    int used = 0;
    uint32_t announced = mqtt_packet_decode_remaining_length(wire + 1, need - 1, &used);
    if (announced != body || 1 + (size_t)used + body != need) __builtin_trap();
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
static void check_connect(const mqtt_connect_opts_t *opts, const uint8_t *wire,
                          size_t need, size_t body) {
    static const uint8_t protocol[] = {0, 4, 'M', 'Q', 'T', 'T', 4};
    uint8_t flags = (uint8_t)((opts->clean_session ? 0x02 : 0) | (opts->username ? 0x80 : 0) |
                              (opts->password ? 0x40 : 0));
    const uint8_t *p = wire + 1 + remaining_field(body);

    check_announced(wire, need, body);
    if (wire[0] != 0x10 || memcmp(p, protocol, sizeof(protocol)) != 0 || p[7] != flags ||
        p[8] != (uint8_t)(opts->keepalive_s >> 8) || p[9] != (uint8_t)opts->keepalive_s) {
        __builtin_trap();
    }
    p = next_string(p + 10, opts->client_id);
    if (opts->username) p = next_string(p, opts->username);
    if (opts->password) p = next_string(p, opts->password);
    if (p != wire + need) __builtin_trap();
}

static void check_subscribe(const uint8_t *wire, size_t need, size_t body,
                            uint16_t id, const char *topic, uint8_t qos) {
    const uint8_t *p = wire + 1 + remaining_field(body);

    check_announced(wire, need, body);
    if (wire[0] != 0x82 || p[0] != (uint8_t)(id >> 8) || p[1] != (uint8_t)id) __builtin_trap();
    p = next_string(p + 2, topic);
    if (p[0] != qos || p + 1 != wire + need) __builtin_trap();
}

static void check_remaining_length(uint32_t value) {
    uint8_t *buf = malloc(4);
    if (!buf) return;

    int n = mqtt_packet_encode_remaining_length(buf, value);
    if (value > 268435455u) {
        if (n >= 0) __builtin_trap();
    } else {
        int used = 0;
        if (n < 1 || n > 4) __builtin_trap();
        if (mqtt_packet_decode_remaining_length(buf, (size_t)n, &used) != value ||
            used != n) {
            __builtin_trap();
        }
    }
    free(buf);
}

static void check_publish_fields(const mqtt_publish_t *pub, const uint8_t *wire, int len) {
    mqtt_publish_t back;
    memset(&back, 0, sizeof(back));
    if (mqtt_packet_parse_publish(wire, (size_t)len, &back) != MQTT_PACKET_OK ||
        back.qos != pub->qos || back.retain != pub->retain || back.dup != pub->dup ||
        back.topic_len != strlen(pub->topic) ||
        memcmp(back.topic, pub->topic, back.topic_len) != 0 ||
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

    mqtt_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.topic = topic;
    pub.payload_len = fuzz_take(in, payload, payload_want);
    pub.payload = pub.payload_len > 0 ? payload : NULL;
    pub.packet_id = fuzz_u16(in);
    pub.qos = op & 0x03;
    pub.retain = (op & 0x04) != 0;
    pub.dup = (op & 0x08) != 0;

    // Exact-size frame, so ASan sees any write past the end.
    size_t body = 2 + topic_len + pub.payload_len + (pub.qos > 0 ? 2 : 0);
    size_t need = 1 + remaining_field(body) + body;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    if (mqtt_packet_build_publish(wire, need - 1, &pub) >= 0) __builtin_trap();

    // A length with no bytes would announce a payload the frame does not hold.
    if (pub.payload_len > 0) {
        pub.payload = NULL;
        if (mqtt_packet_build_publish(wire, need, &pub) >= 0) __builtin_trap();
        pub.payload = payload;
    }

    int len = mqtt_packet_build_publish(wire, need, &pub);
    if (pub.qos > 2) {
        if (len >= 0) __builtin_trap();
    } else {
        if (len != (int)need) __builtin_trap();
        check_announced(wire, need, body);
        check_publish_fields(&pub, wire, len);
    }
    free(wire);
}

static void round_trip_connect(fuzz_input_t *in) {
    char client[STR_MAX + 1], user[STR_MAX + 1], pass[STR_MAX + 1];
    uint8_t op = fuzz_u8(in);
    size_t want = fuzz_u8(in) % (STR_MAX + 1);
    size_t body = 10 + 2 + fuzz_take_str(in, client, want);

    mqtt_connect_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.client_id = client;
    opts.keepalive_s = fuzz_u16(in);
    opts.clean_session = (op & 0x01) != 0;
    if (op & 0x02) {
        want = fuzz_u8(in) % (STR_MAX + 1);
        body += 2 + fuzz_take_str(in, user, want);
        opts.username = user;
    }
    if (op & 0x04) {
        want = fuzz_u8(in) % (STR_MAX + 1);
        body += 2 + fuzz_take_str(in, pass, want);
        opts.password = pass;
    }

    size_t need = 1 + remaining_field(body) + body;
    uint8_t *wire = malloc(need);
    if (!wire) return;

    if (mqtt_packet_build_connect(wire, need - 1, &opts) >= 0) __builtin_trap();

    int len = mqtt_packet_build_connect(wire, need, &opts);
    if (opts.password && !opts.username) {
        if (len >= 0) __builtin_trap();
    } else {
        if (len != (int)need || mqtt_packet_get_type(wire, need) != MQTT_CONNECT) {
            __builtin_trap();
        }
        check_connect(&opts, wire, need, body);
    }
    free(wire);
}

static void round_trip_subscribe(fuzz_input_t *in) {
    char topic[STR_MAX + 1];
    uint8_t qos = fuzz_u8(in) % 5;
    uint16_t id = fuzz_u16(in);
    size_t want = fuzz_u8(in) % (STR_MAX + 1);
    size_t body = 2 + 2 + fuzz_take_str(in, topic, want) + 1;
    size_t need = 1 + remaining_field(body) + body;

    uint8_t *wire = malloc(need);
    if (!wire) return;

    if (mqtt_packet_build_subscribe(wire, need - 1, id, topic, qos) >= 0) __builtin_trap();

    // A SUBSCRIBE asks for QoS 0 to 2; anything else is a protocol error.
    int len = mqtt_packet_build_subscribe(wire, need, id, topic, qos);
    if ((qos > 2) != (len < 0)) __builtin_trap();
    if (len >= 0) {
        if (len != (int)need || mqtt_packet_get_type(wire, need) != MQTT_SUBSCRIBE) {
            __builtin_trap();
        }
        check_subscribe(wire, need, body, id, topic, qos);
    }
    free(wire);
}

// A string carries a 16-bit length, so one byte more than that must be refused.
static void check_long_strings(uint8_t extra) {
    static char text[UINT16_MAX + 16];
    size_t len = UINT16_MAX - 7 + extra % 16;
    memset(text, 't', len);
    text[len] = 0;

    size_t room = len + 32;
    uint8_t *wire = malloc(room);
    if (!wire) return;

    mqtt_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.topic = text;

    mqtt_connect_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.client_id = text;

    bool fits = len <= UINT16_MAX;
    if ((mqtt_packet_build_publish(wire, room, &pub) >= 0) != fits) __builtin_trap();
    if ((mqtt_packet_build_subscribe(wire, room, 1, text, 0) >= 0) != fits) __builtin_trap();
    if ((mqtt_packet_build_connect(wire, room, &opts) >= 0) != fits) __builtin_trap();
    free(wire);
}

static void check_two_byte_packets(void) {
    uint8_t *wire = malloc(2);
    if (!wire) return;

    if (mqtt_packet_build_pingreq(wire, 1) >= 0) __builtin_trap();
    if (mqtt_packet_build_pingreq(wire, 2) != 2 || wire[0] != 0xC0 || wire[1] != 0) {
        __builtin_trap();
    }
    if (mqtt_packet_build_disconnect(wire, 1) >= 0) __builtin_trap();
    if (mqtt_packet_build_disconnect(wire, 2) != 2 || wire[0] != 0xE0 || wire[1] != 0) {
        __builtin_trap();
    }
    free(wire);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Exercise all parse functions with fuzz input

    // Get packet type
    mqtt_packet_get_type(data, size);

    // Parse CONNACK
    mqtt_connack_t connack;
    memset(&connack, 0, sizeof(connack));
    mqtt_packet_parse_connack(data, size, &connack);

    // Parse PUBLISH
    mqtt_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    if (mqtt_packet_parse_publish(data, size, &pub) == MQTT_PACKET_OK && pub.qos > 2) {
        __builtin_trap();
    }

    // Parse PUBACK
    uint16_t packet_id;
    mqtt_packet_parse_puback(data, size, &packet_id);

    // Decode remaining length
    int bytes_used = 0;
    mqtt_packet_decode_remaining_length(data, size, &bytes_used);

    fuzz_input_t in;
    fuzz_input_init(&in, data, size);
    check_two_byte_packets();
    check_long_strings(size > 0 ? data[0] : 0);
    while (in.left > 0) {
        uint32_t hi = fuzz_u16(&in);
        check_remaining_length((hi << 16) | fuzz_u16(&in));
        round_trip_publish(&in);
        round_trip_connect(&in);
        round_trip_subscribe(&in);
    }

    return 0;
}
