/*
 * prop_mqtt_packet.c — property-based tests for mqtt_packet
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_mqtt_packet.c ../mqtt_packet.c -o prop_mqtt_packet
 */

#include "test_harness.h"
#include "property_test.h"
#include "../mqtt_packet.h"
#include <string.h>

/* Property: PUBLISH build then parse roundtrip preserves topic + payload */
PROPERTY(publish_roundtrip, 10000) {
    /* Generate a short topic (1..30 chars, printable ASCII) */
    char topic[32];
    uint8_t topic_len = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < topic_len; i++)
        topic[i] = (char)ARBI_RANGE(seed, 0x20, 0x7E);
    topic[topic_len] = '\0';

    /* Generate payload */
    uint8_t payload[128];
    uint8_t payload_len = (uint8_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, payload, payload_len);

    uint8_t qos = (uint8_t)ARBI_RANGE(seed, 0, 1); /* QoS 0 or 1 */
    uint16_t pkt_id = (qos > 0) ? ARBI_UINT16(seed) | 1u : 0;

    mqtt_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.topic = topic;
    pub.payload = payload;
    pub.payload_len = payload_len;
    pub.packet_id = pkt_id;
    pub.qos = qos;
    pub.retain = false;
    pub.dup = false;

    uint8_t buf[512];
    int built = mqtt_packet_build_publish(buf, sizeof(buf), &pub);
    PROP_ASSERT(built > 0);

    mqtt_publish_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    mqtt_packet_err_t err = mqtt_packet_parse_publish(buf, (size_t)built, &parsed);
    PROP_ASSERT_EQ(err, MQTT_PACKET_OK);

    /* Topic must match (zero-copy pointer, not NUL-terminated) */
    PROP_ASSERT(parsed.topic != NULL);
    PROP_ASSERT(memcmp(parsed.topic, topic, topic_len) == 0);

    /* Payload must match */
    PROP_ASSERT_EQ(parsed.payload_len, payload_len);
    if (payload_len > 0) {
        PROP_ASSERT_MEM_EQ(parsed.payload, payload, payload_len);
    }

    /* QoS and packet ID must match */
    PROP_ASSERT_EQ(parsed.qos, qos);
    if (qos > 0) {
        PROP_ASSERT_EQ(parsed.packet_id, pkt_id);
    }
}

/* Property: CONNECT build produces deterministic output */
PROPERTY(connect_determinism, 5000) {
    char client_id[16];
    uint8_t id_len = (uint8_t)ARBI_RANGE(seed, 1, 15);
    for (uint8_t i = 0; i < id_len; i++)
        client_id[i] = (char)ARBI_RANGE(seed, 'a', 'z');
    client_id[id_len] = '\0';

    uint16_t keepalive = ARBI_UINT16(seed);

    mqtt_connect_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.client_id = client_id;
    opts.keepalive_s = keepalive;
    opts.clean_session = true;

    uint8_t buf1[256], buf2[256];
    int len1 = mqtt_packet_build_connect(buf1, sizeof(buf1), &opts);
    int len2 = mqtt_packet_build_connect(buf2, sizeof(buf2), &opts);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: SUBSCRIBE build returns positive length, type byte is correct */
PROPERTY(subscribe_length_consistency, 5000) {
    char topic[20];
    uint8_t tlen = (uint8_t)ARBI_RANGE(seed, 1, 19);
    for (uint8_t i = 0; i < tlen; i++)
        topic[i] = (char)ARBI_RANGE(seed, 'a', 'z');
    topic[tlen] = '\0';

    uint16_t pkt_id = ARBI_UINT16(seed) | 1u;
    uint8_t qos = (uint8_t)ARBI_RANGE(seed, 0, 2);

    uint8_t buf[256];
    int built = mqtt_packet_build_subscribe(buf, sizeof(buf),
                                            pkt_id, topic, qos);
    PROP_ASSERT(built > 0);

    /* First byte high nibble should be SUBSCRIBE type (0x80) */
    mqtt_packet_type_t ptype = mqtt_packet_get_type(buf, (size_t)built);
    PROP_ASSERT_EQ(ptype, MQTT_SUBSCRIBE);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[300];
    size_t len = ARBI_SIZE(seed, 300);
    ARBI_FILL(seed, garbage, len);

    /* Try all parse functions — none should crash */
    mqtt_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    mqtt_packet_parse_publish(garbage, len, &pub);

    mqtt_connack_t connack;
    mqtt_packet_parse_connack(garbage, len, &connack);

    uint16_t pkt_id;
    mqtt_packet_parse_puback(garbage, len, &pkt_id);

    mqtt_packet_get_type(garbage, len);

    if (len > 1) {
        int bytes_used;
        mqtt_packet_decode_remaining_length(garbage + 1, len - 1, &bytes_used);
    }
}

/* Property: PINGREQ and DISCONNECT build produce fixed small packets */
PROPERTY(fixed_packets_stable, 1000) {
    (void)seed; (void)prop__seed_orig; (void)prop__iter;
    uint8_t ping1[8], ping2[8], disc1[8], disc2[8];
    int p1 = mqtt_packet_build_pingreq(ping1, sizeof(ping1));
    int p2 = mqtt_packet_build_pingreq(ping2, sizeof(ping2));
    PROP_ASSERT(p1 > 0);
    PROP_ASSERT_EQ(p1, p2);
    PROP_ASSERT_MEM_EQ(ping1, ping2, (size_t)p1);

    int d1 = mqtt_packet_build_disconnect(disc1, sizeof(disc1));
    int d2 = mqtt_packet_build_disconnect(disc2, sizeof(disc2));
    PROP_ASSERT(d1 > 0);
    PROP_ASSERT_EQ(d1, d2);
    PROP_ASSERT_MEM_EQ(disc1, disc2, (size_t)d1);
}

TEST_MAIN("mqtt_packet properties") {
    RUN(publish_roundtrip);
    RUN(connect_determinism);
    RUN(subscribe_length_consistency);
    RUN(parse_no_crash);
    RUN(fixed_packets_stable);
}
TEST_MAIN_END()
