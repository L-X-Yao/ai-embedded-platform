/*
 * prop_mqtt5_packet.c — property-based tests for mqtt5_packet
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_mqtt5_packet.c ../mqtt5_packet.c -o prop_mqtt5_packet
 */

#include "test_harness.h"
#include "property_test.h"
#include "../mqtt5_packet.h"
#include <string.h>

/* Property: PUBLISH build then parse roundtrip preserves topic + payload */
PROPERTY(publish_roundtrip, 10000) {
    char topic[32];
    uint8_t topic_len = (uint8_t)ARBI_RANGE(seed, 1, 30);
    for (uint8_t i = 0; i < topic_len; i++)
        topic[i] = (char)ARBI_RANGE(seed, 0x20, 0x7E);
    topic[topic_len] = '\0';

    uint8_t payload[128];
    uint8_t payload_len = (uint8_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, payload, payload_len);

    mqtt5_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.topic = topic;
    pub.payload = payload;
    pub.payload_len = payload_len;
    pub.qos = (uint8_t)ARBI_RANGE(seed, 0, 1);
    pub.packet_id = (pub.qos > 0) ? (ARBI_UINT16(seed) | 1u) : 0;
    pub.retain = false;
    pub.message_expiry = 0;

    uint8_t buf[512];
    int built = mqtt5_build_publish(buf, sizeof(buf), &pub);
    PROP_ASSERT(built > 0);

    mqtt5_publish_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    mqtt5_err_t err = mqtt5_parse_publish(buf, (uint16_t)built, &parsed);
    PROP_ASSERT_EQ(err, MQTT5_OK);

    PROP_ASSERT(parsed.topic != NULL);
    PROP_ASSERT(memcmp(parsed.topic, topic, topic_len) == 0);
    PROP_ASSERT_EQ(parsed.payload_len, payload_len);
    if (payload_len > 0) {
        PROP_ASSERT_MEM_EQ(parsed.payload, payload, payload_len);
    }
    PROP_ASSERT_EQ(parsed.qos, pub.qos);
}

/* Property: CONNECT build produces deterministic output */
PROPERTY(connect_determinism, 5000) {
    char client_id[16];
    uint8_t id_len = (uint8_t)ARBI_RANGE(seed, 1, 15);
    for (uint8_t i = 0; i < id_len; i++)
        client_id[i] = (char)ARBI_RANGE(seed, 'a', 'z');
    client_id[id_len] = '\0';

    mqtt5_connect_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.client_id = client_id;
    cfg.keep_alive = ARBI_UINT16(seed);
    cfg.clean_start = true;
    cfg.session_expiry = 0;

    uint8_t buf1[256], buf2[256];
    int len1 = mqtt5_build_connect(buf1, sizeof(buf1), &cfg);
    int len2 = mqtt5_build_connect(buf2, sizeof(buf2), &cfg);
    PROP_ASSERT(len1 > 0);
    PROP_ASSERT_EQ(len1, len2);
    PROP_ASSERT_MEM_EQ(buf1, buf2, (size_t)len1);
}

/* Property: VBI encode/decode roundtrip */
PROPERTY(vbi_roundtrip, 20000) {
    uint32_t value = ARBI_UINT32(seed) % 268435456u; /* max VBI = 0x0FFFFFFF */
    uint8_t buf[4];
    int enc_len = mqtt5_encode_vbi(buf, value);
    PROP_ASSERT(enc_len >= 1 && enc_len <= 4);

    uint32_t decoded = 0;
    uint8_t bytes_used = 0;
    mqtt5_err_t err = mqtt5_decode_vbi(buf, (uint16_t)enc_len, &decoded, &bytes_used);
    PROP_ASSERT_EQ(err, MQTT5_OK);
    PROP_ASSERT_EQ(decoded, value);
    PROP_ASSERT_EQ(bytes_used, enc_len);
}

/* Property: parse of arbitrary bytes never crashes */
PROPERTY(parse_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t garbage[300];
    size_t len = ARBI_SIZE(seed, 300);
    ARBI_FILL(seed, garbage, len);

    mqtt5_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    mqtt5_parse_publish(garbage, (uint16_t)(len > 65535 ? 65535 : len), &pub);

    mqtt5_connack_t ack;
    memset(&ack, 0, sizeof(ack));
    mqtt5_parse_connack(garbage, (uint16_t)(len > 65535 ? 65535 : len), &ack);

    uint32_t val = 0;
    uint8_t used = 0;
    mqtt5_decode_vbi(garbage, (uint16_t)(len > 65535 ? 65535 : len), &val, &used);
}

TEST_MAIN("mqtt5_packet properties") {
    RUN(publish_roundtrip);
    RUN(connect_determinism);
    RUN(vbi_roundtrip);
    RUN(parse_no_crash);
}
TEST_MAIN_END()
