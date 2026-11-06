// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include <stdlib.h>
#include "mqtt_packet.h"
TEST(test_remaining_length_encode)
{
    uint8_t buf[4];

    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 0), 1);
    ASSERT_EQ(buf[0], 0x00);

    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 127), 1);
    ASSERT_EQ(buf[0], 0x7F);

    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 128), 2);
    ASSERT_EQ(buf[0], 0x80);
    ASSERT_EQ(buf[1], 0x01);

    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 16383), 2);
    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 16384), 3);
}

TEST(test_remaining_length_decode)
{
    int used = 0;

    uint8_t b1[] = { 0x00 };
    ASSERT_EQ(mqtt_packet_decode_remaining_length(b1, 1, &used), 0);
    ASSERT_EQ(used, 1);

    uint8_t b2[] = { 0x7F };
    ASSERT_EQ(mqtt_packet_decode_remaining_length(b2, 1, &used), 127);

    uint8_t b3[] = { 0x80, 0x01 };
    ASSERT_EQ(mqtt_packet_decode_remaining_length(b3, 2, &used), 128);
    ASSERT_EQ(used, 2);
}

TEST(test_build_connect)
{
    uint8_t buf[128];
    mqtt_connect_opts_t opts = {
        .client_id = "test",
        .username = NULL,
        .password = NULL,
        .keepalive_s = 60,
        .clean_session = true,
    };
    int len = mqtt_packet_build_connect(buf, sizeof(buf), &opts);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[0] >> 4, MQTT_CONNECT);
    ASSERT_TRUE(memcmp(buf + 4, "MQTT", 4) == 0);
    ASSERT_EQ(buf[8], 0x04);
    ASSERT_EQ(buf[9] & 0x02, 0x02);
}

TEST(test_build_connect_with_auth)
{
    uint8_t buf[256];
    mqtt_connect_opts_t opts = {
        .client_id = "dev1",
        .username = "user",
        .password = "pass",
        .keepalive_s = 120,
        .clean_session = true,
    };
    int len = mqtt_packet_build_connect(buf, sizeof(buf), &opts);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[9] & 0xC0, 0xC0);
}

TEST(test_build_connect_buf_too_small)
{
    uint8_t buf[5];
    mqtt_connect_opts_t opts = { .client_id = "test", .keepalive_s = 60 };
    int len = mqtt_packet_build_connect(buf, sizeof(buf), &opts);
    ASSERT_EQ(len, MQTT_PACKET_ERR_SIZE);
}

TEST(test_build_publish_qos0)
{
    uint8_t buf[64];
    uint8_t payload[] = "hello";
    mqtt_publish_t pub = {
        .topic = "t/1",
        .payload = payload,
        .payload_len = 5,
        .qos = 0,
        .retain = false,
        .dup = false,
    };
    int len = mqtt_packet_build_publish(buf, sizeof(buf), &pub);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[0] >> 4, MQTT_PUBLISH);
    ASSERT_EQ(buf[0] & 0x06, 0);
    ASSERT_EQ(buf[0] & 0x01, 0);
}

TEST(test_build_publish_qos1_retain)
{
    uint8_t buf[64];
    mqtt_publish_t pub = {
        .topic = "t/2",
        .payload = (const uint8_t *)"data",
        .payload_len = 4,
        .packet_id = 42,
        .qos = 1,
        .retain = true,
        .dup = false,
    };
    int len = mqtt_packet_build_publish(buf, sizeof(buf), &pub);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[0] & 0x06, 0x02);
    ASSERT_EQ(buf[0] & 0x01, 0x01);
}

TEST(test_build_subscribe)
{
    uint8_t buf[64];
    int len = mqtt_packet_build_subscribe(buf, sizeof(buf), 1, "sensor/temp", 1);
    ASSERT_TRUE(len > 0);
    ASSERT_EQ(buf[0] >> 4, MQTT_SUBSCRIBE);
    ASSERT_EQ(buf[0] & 0x0F, 0x02);
}

TEST(test_build_pingreq)
{
    uint8_t buf[4];
    int len = mqtt_packet_build_pingreq(buf, sizeof(buf));
    ASSERT_EQ(len, 2);
    ASSERT_EQ(buf[0], (MQTT_PINGREQ << 4));
    ASSERT_EQ(buf[1], 0);
}

TEST(test_build_disconnect)
{
    uint8_t buf[4];
    int len = mqtt_packet_build_disconnect(buf, sizeof(buf));
    ASSERT_EQ(len, 2);
    ASSERT_EQ(buf[0], (MQTT_DISCONNECT << 4));
}

TEST(test_get_type)
{
    uint8_t connack[] = { 0x20, 0x02, 0x00, 0x00 };
    ASSERT_EQ(mqtt_packet_get_type(connack, 4), MQTT_CONNACK);

    ASSERT_EQ(mqtt_packet_get_type(NULL, 0), 0);
}

TEST(test_parse_connack)
{
    uint8_t pkt[] = { 0x20, 0x02, 0x01, 0x00 };
    mqtt_connack_t ca;
    ASSERT_EQ(mqtt_packet_parse_connack(pkt, 4, &ca), MQTT_PACKET_OK);
    ASSERT_TRUE(ca.session_present);
    ASSERT_EQ(ca.return_code, 0);
}

TEST(test_parse_connack_rejected)
{
    uint8_t pkt[] = { 0x20, 0x02, 0x00, 0x05 };
    mqtt_connack_t ca;
    mqtt_packet_parse_connack(pkt, 4, &ca);
    ASSERT_EQ(ca.return_code, 5);
}

TEST(test_parse_puback)
{
    uint8_t pkt[] = { 0x40, 0x02, 0x00, 0x2A };
    uint16_t pid;
    ASSERT_EQ(mqtt_packet_parse_puback(pkt, 4, &pid), MQTT_PACKET_OK);
    ASSERT_EQ(pid, 42);
}

TEST(test_roundtrip_publish)
{
    uint8_t buf[128];
    uint8_t payload[] = "temp=25.3";
    mqtt_publish_t pub_out = {
        .topic = "sensor/data",
        .payload = payload,
        .payload_len = 9,
        .qos = 0,
        .retain = false,
    };
    int len = mqtt_packet_build_publish(buf, sizeof(buf), &pub_out);
    ASSERT_TRUE(len > 0);

    mqtt_publish_t pub_in = {0};
    ASSERT_EQ(mqtt_packet_parse_publish(buf, (size_t)len, &pub_in), MQTT_PACKET_OK);
    ASSERT_TRUE(strncmp(pub_in.topic, "sensor/data", 11) == 0);
    ASSERT_EQ(pub_in.payload_len, 9);
    ASSERT_TRUE(memcmp(pub_in.payload, "temp=25.3", 9) == 0);
    ASSERT_EQ(pub_in.qos, 0);
}

TEST(test_roundtrip_publish_qos1)
{
    uint8_t buf[128];
    mqtt_publish_t pub_out = {
        .topic = "cmd/ack",
        .payload = (const uint8_t *)"ok",
        .payload_len = 2,
        .qos = 1,
        .packet_id = 42,
        .retain = false,
    };
    int len = mqtt_packet_build_publish(buf, sizeof(buf), &pub_out);
    ASSERT_TRUE(len > 0);

    mqtt_publish_t pub_in = {0};
    ASSERT_EQ(mqtt_packet_parse_publish(buf, (size_t)len, &pub_in), MQTT_PACKET_OK);
    ASSERT_EQ(pub_in.qos, 1);
    ASSERT_EQ(pub_in.packet_id, 42);
    ASSERT_EQ(pub_in.payload_len, 2);
    ASSERT_TRUE(memcmp(pub_in.payload, "ok", 2) == 0);
}

TEST(test_remaining_length_4byte)
{
    uint8_t buf[4];
    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 2097152), 4);

    int used = 0;
    uint32_t val = mqtt_packet_decode_remaining_length(buf, 4, &used);
    ASSERT_EQ(val, 2097152);
    ASSERT_EQ(used, 4);

    uint8_t bad[] = { 0x80, 0x80, 0x80, 0x80 };
    val = mqtt_packet_decode_remaining_length(bad, 4, &used);
    ASSERT_EQ(used, 0);
}

TEST(test_parse_malformed_type)
{
    uint8_t pub[] = {0x30, 0x02, 0x00, 0x00};
    mqtt_connack_t ca;
    ASSERT_EQ(mqtt_packet_parse_connack(pub, sizeof(pub), &ca),
              MQTT_PACKET_ERR_MALFORMED);

    uint16_t pid;
    ASSERT_EQ(mqtt_packet_parse_puback(pub, sizeof(pub), &pid),
              MQTT_PACKET_ERR_MALFORMED);
}

TEST(test_build_publish_qos3_rejected)
{
    uint8_t buf[64];
    mqtt_publish_t pub = {
        .topic = "t/1", .payload = (const uint8_t *)"x",
        .payload_len = 1, .qos = 3,
    };
    ASSERT_EQ(mqtt_packet_build_publish(buf, sizeof(buf), &pub),
              MQTT_PACKET_ERR_PARAM);
}

TEST(test_null_params)
{
    ASSERT_EQ(mqtt_packet_build_connect(NULL, 128, NULL), MQTT_PACKET_ERR_PARAM);
    ASSERT_EQ(mqtt_packet_build_publish(NULL, 128, NULL), MQTT_PACKET_ERR_PARAM);
    ASSERT_EQ(mqtt_packet_build_subscribe(NULL, 128, 1, NULL, 0), MQTT_PACKET_ERR_PARAM);
    ASSERT_EQ(mqtt_packet_build_pingreq(NULL, 2), MQTT_PACKET_ERR_SIZE);

    mqtt_connack_t ca;
    ASSERT_EQ(mqtt_packet_parse_connack(NULL, 4, &ca), MQTT_PACKET_ERR_PARAM);
    uint16_t pid;
    ASSERT_EQ(mqtt_packet_parse_puback(NULL, 4, &pid), MQTT_PACKET_ERR_PARAM);
}

TEST(test_build_connect_string_exceeds_uint16_max)
{
    size_t huge = (size_t)UINT16_MAX + 2;
    char *big = (char *)malloc(huge);
    ASSERT_TRUE(big != NULL);
    memset(big, 'A', huge - 1);
    big[huge - 1] = '\0';
    uint8_t buf[32];
    mqtt_connect_opts_t opts = { .client_id = big, .keepalive_s = 60 };
    ASSERT_EQ(mqtt_packet_build_connect(buf, sizeof(buf), &opts),
              MQTT_PACKET_ERR_SIZE);
    free(big);
}

TEST(test_build_publish_over_protocol_limit)
{
    /* 268435455 is the largest remaining length MQTT can encode. The frame
     * is refused before anything is copied, so the size given here does not
     * have to be real. */
    uint8_t buf[8];
    uint8_t payload[1] = {0};
    mqtt_publish_t pub = { .topic = "t", .payload = payload, .payload_len = 268435456u };

    ASSERT_EQ(mqtt_packet_build_publish(buf, SIZE_MAX, &pub), MQTT_PACKET_ERR_SIZE);
}

TEST(test_build_connect_long_string_writes_nothing)
{
    /* Each string has a 16-bit length, so the limit is checked up front. */
    char *big = malloc(65537);
    ASSERT_NOT_NULL(big);
    memset(big, 'a', 65536);
    big[65536] = 0;

    uint8_t buf[16];
    memset(buf, 0xEE, sizeof(buf));

    mqtt_connect_opts_t opts = { .client_id = big };
    ASSERT_EQ(mqtt_packet_build_connect(buf, SIZE_MAX, &opts), MQTT_PACKET_ERR_SIZE);

    opts.client_id = "c";
    opts.username = big;
    ASSERT_EQ(mqtt_packet_build_connect(buf, SIZE_MAX, &opts), MQTT_PACKET_ERR_SIZE);

    opts.username = "u";
    opts.password = big;
    ASSERT_EQ(mqtt_packet_build_connect(buf, SIZE_MAX, &opts), MQTT_PACKET_ERR_SIZE);

    ASSERT_EQ(buf[0], 0xEE);
    free(big);
}

TEST(test_build_subscribe_qos3_rejected)
{
    /* MQTT 3.1.1 section 3.8.3: a requested QoS of 3 is a protocol violation. */
    uint8_t buf[32];

    ASSERT_EQ(mqtt_packet_build_subscribe(buf, sizeof(buf), 1, "t", 3), MQTT_PACKET_ERR_PARAM);
    ASSERT_TRUE(mqtt_packet_build_subscribe(buf, sizeof(buf), 1, "t", 2) > 0);
}

TEST(test_build_publish_length_without_payload)
{
    /* A length with no bytes would announce a payload the frame does not hold. */
    uint8_t buf[32];
    mqtt_publish_t pub = { .topic = "t", .payload = NULL, .payload_len = 4 };

    ASSERT_EQ(mqtt_packet_build_publish(buf, sizeof(buf), &pub), MQTT_PACKET_ERR_PARAM);

    pub.payload_len = 0;
    ASSERT_TRUE(mqtt_packet_build_publish(buf, sizeof(buf), &pub) > 0);
}

TEST(test_parse_publish_refuses_both_qos_bits)
{
    const uint8_t frame[] = { 0x36, 0x05, 0x00, 0x01, 't', 0x00, 0x01 };
    mqtt_publish_t pub;

    ASSERT_EQ(mqtt_packet_parse_publish(frame, sizeof(frame), &pub), MQTT_PACKET_ERR_MALFORMED);
}

/* Header 2, topic length 2, topic 3, payload 10: 17 bytes, remaining 15. */
static const uint8_t publish_17[] = {
    0x30, 0x0F, 0x00, 0x03, 'a', '/', 'b',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
};

TEST(test_parse_publish_refuses_a_cut_packet)
{
    mqtt_publish_t pub;

    /* Below 4 bytes is ERR_PARAM, so every cut from there up has to fail. */
    for (size_t len = 4; len < sizeof(publish_17); len++) {
        ASSERT_EQ(mqtt_packet_parse_publish(publish_17, len, &pub),
                  MQTT_PACKET_ERR_MALFORMED);
    }
}

TEST(test_parse_publish_takes_the_whole_packet)
{
    mqtt_publish_t pub;

    ASSERT_EQ(mqtt_packet_parse_publish(publish_17, sizeof(publish_17), &pub),
              MQTT_PACKET_OK);
    ASSERT_EQ(pub.topic_len, 3);
    ASSERT_TRUE(memcmp(pub.topic, "a/b", 3) == 0);
    ASSERT_EQ(pub.payload_len, 10);
    ASSERT_TRUE(memcmp(pub.payload, "0123456789", 10) == 0);
}

TEST(test_parse_publish_stops_at_the_remaining_length)
{
    uint8_t stream[sizeof(publish_17) + 2];
    mqtt_publish_t pub;

    /* Bytes of the next packet follow, and they are not this payload. */
    memcpy(stream, publish_17, sizeof(publish_17));
    stream[sizeof(publish_17)] = 0xC0;
    stream[sizeof(publish_17) + 1] = 0x00;

    ASSERT_EQ(mqtt_packet_parse_publish(stream, sizeof(stream), &pub),
              MQTT_PACKET_OK);
    ASSERT_EQ(pub.payload_len, 10);
}

TEST_MAIN("mqtt_packet")
    RUN(test_parse_publish_refuses_a_cut_packet);
    RUN(test_parse_publish_takes_the_whole_packet);
    RUN(test_parse_publish_stops_at_the_remaining_length);
    RUN(test_remaining_length_encode);
    RUN(test_remaining_length_decode);
    RUN(test_build_connect);
    RUN(test_build_connect_with_auth);
    RUN(test_build_connect_buf_too_small);
    RUN(test_build_publish_qos0);
    RUN(test_build_publish_qos1_retain);
    RUN(test_build_subscribe);
    RUN(test_build_pingreq);
    RUN(test_build_disconnect);
    RUN(test_get_type);
    RUN(test_parse_connack);
    RUN(test_parse_connack_rejected);
    RUN(test_parse_puback);
    RUN(test_roundtrip_publish);
    RUN(test_roundtrip_publish_qos1);
    RUN(test_remaining_length_4byte);
    RUN(test_build_publish_qos3_rejected);
    RUN(test_parse_malformed_type);
    RUN(test_null_params);
    RUN(test_build_connect_string_exceeds_uint16_max);
    RUN(test_build_publish_over_protocol_limit);
    RUN(test_build_connect_long_string_writes_nothing);
    RUN(test_build_subscribe_qos3_rejected);
    RUN(test_build_publish_length_without_payload);
    RUN(test_parse_publish_refuses_both_qos_bits);
TEST_MAIN_END()

