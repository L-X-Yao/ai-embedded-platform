#include <stdio.h>
#include <string.h>
#include "../mqtt_packet.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((int)(a) == (int)(b), msg)

static void test_remaining_length_encode(void)
{
    printf("%s\n", __func__);
    uint8_t buf[4];

    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 0), 1, "0 -> 1 byte");
    ASSERT_EQ(buf[0], 0x00, "0 encoded");

    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 127), 1, "127 -> 1 byte");
    ASSERT_EQ(buf[0], 0x7F, "127 encoded");

    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 128), 2, "128 -> 2 bytes");
    ASSERT_EQ(buf[0], 0x80, "128 byte 0");
    ASSERT_EQ(buf[1], 0x01, "128 byte 1");

    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 16383), 2, "16383 -> 2 bytes");
    ASSERT_EQ(mqtt_packet_encode_remaining_length(buf, 16384), 3, "16384 -> 3 bytes");
}

static void test_remaining_length_decode(void)
{
    printf("%s\n", __func__);
    int used = 0;

    uint8_t b1[] = { 0x00 };
    ASSERT_EQ(mqtt_packet_decode_remaining_length(b1, 1, &used), 0, "decode 0");
    ASSERT_EQ(used, 1, "used 1 byte");

    uint8_t b2[] = { 0x7F };
    ASSERT_EQ(mqtt_packet_decode_remaining_length(b2, 1, &used), 127, "decode 127");

    uint8_t b3[] = { 0x80, 0x01 };
    ASSERT_EQ(mqtt_packet_decode_remaining_length(b3, 2, &used), 128, "decode 128");
    ASSERT_EQ(used, 2, "used 2 bytes");
}

static void test_build_connect(void)
{
    printf("%s\n", __func__);
    uint8_t buf[128];
    mqtt_connect_opts_t opts = {
        .client_id = "test",
        .username = NULL,
        .password = NULL,
        .keepalive_s = 60,
        .clean_session = true,
    };
    int len = mqtt_packet_build_connect(buf, sizeof(buf), &opts);
    ASSERT(len > 0, "connect built");
    ASSERT_EQ(buf[0] >> 4, MQTT_CONNECT, "type=CONNECT");
    ASSERT(memcmp(buf + 4, "MQTT", 4) == 0, "protocol name");
    ASSERT_EQ(buf[8], 0x04, "protocol level 4");
    ASSERT_EQ(buf[9] & 0x02, 0x02, "clean session flag");
}

static void test_build_connect_with_auth(void)
{
    printf("%s\n", __func__);
    uint8_t buf[256];
    mqtt_connect_opts_t opts = {
        .client_id = "dev1",
        .username = "user",
        .password = "pass",
        .keepalive_s = 120,
        .clean_session = true,
    };
    int len = mqtt_packet_build_connect(buf, sizeof(buf), &opts);
    ASSERT(len > 0, "connect with auth built");
    ASSERT_EQ(buf[9] & 0xC0, 0xC0, "username+password flags");
}

static void test_build_connect_buf_too_small(void)
{
    printf("%s\n", __func__);
    uint8_t buf[5];
    mqtt_connect_opts_t opts = { .client_id = "test", .keepalive_s = 60 };
    int len = mqtt_packet_build_connect(buf, sizeof(buf), &opts);
    ASSERT_EQ(len, MQTT_PACKET_ERR_SIZE, "buffer too small");
}

static void test_build_publish_qos0(void)
{
    printf("%s\n", __func__);
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
    ASSERT(len > 0, "publish built");
    ASSERT_EQ(buf[0] >> 4, MQTT_PUBLISH, "type=PUBLISH");
    ASSERT_EQ(buf[0] & 0x06, 0, "qos=0");
    ASSERT_EQ(buf[0] & 0x01, 0, "retain=0");
}

static void test_build_publish_qos1_retain(void)
{
    printf("%s\n", __func__);
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
    ASSERT(len > 0, "publish qos1 built");
    ASSERT_EQ(buf[0] & 0x06, 0x02, "qos=1 bits");
    ASSERT_EQ(buf[0] & 0x01, 0x01, "retain bit");
}

static void test_build_subscribe(void)
{
    printf("%s\n", __func__);
    uint8_t buf[64];
    int len = mqtt_packet_build_subscribe(buf, sizeof(buf), 1, "sensor/temp", 1);
    ASSERT(len > 0, "subscribe built");
    ASSERT_EQ(buf[0] >> 4, MQTT_SUBSCRIBE, "type=SUBSCRIBE");
    ASSERT_EQ(buf[0] & 0x0F, 0x02, "subscribe fixed flags");
}

static void test_build_pingreq(void)
{
    printf("%s\n", __func__);
    uint8_t buf[4];
    int len = mqtt_packet_build_pingreq(buf, sizeof(buf));
    ASSERT_EQ(len, 2, "pingreq = 2 bytes");
    ASSERT_EQ(buf[0], (MQTT_PINGREQ << 4), "pingreq byte 0");
    ASSERT_EQ(buf[1], 0, "pingreq byte 1");
}

static void test_build_disconnect(void)
{
    printf("%s\n", __func__);
    uint8_t buf[4];
    int len = mqtt_packet_build_disconnect(buf, sizeof(buf));
    ASSERT_EQ(len, 2, "disconnect = 2 bytes");
    ASSERT_EQ(buf[0], (MQTT_DISCONNECT << 4), "disconnect byte 0");
}

static void test_get_type(void)
{
    printf("%s\n", __func__);
    uint8_t connack[] = { 0x20, 0x02, 0x00, 0x00 };
    ASSERT_EQ(mqtt_packet_get_type(connack, 4), MQTT_CONNACK, "connack type");

    ASSERT_EQ(mqtt_packet_get_type(NULL, 0), 0, "null type");
}

static void test_parse_connack(void)
{
    printf("%s\n", __func__);
    uint8_t pkt[] = { 0x20, 0x02, 0x01, 0x00 };
    mqtt_connack_t ca;
    ASSERT_EQ(mqtt_packet_parse_connack(pkt, 4, &ca), MQTT_PACKET_OK, "connack parsed");
    ASSERT(ca.session_present, "session present");
    ASSERT_EQ(ca.return_code, 0, "return code 0");
}

static void test_parse_connack_rejected(void)
{
    printf("%s\n", __func__);
    uint8_t pkt[] = { 0x20, 0x02, 0x00, 0x05 };
    mqtt_connack_t ca;
    mqtt_packet_parse_connack(pkt, 4, &ca);
    ASSERT_EQ(ca.return_code, 5, "return code 5 = not authorized");
}

static void test_parse_puback(void)
{
    printf("%s\n", __func__);
    uint8_t pkt[] = { 0x40, 0x02, 0x00, 0x2A };
    uint16_t pid;
    ASSERT_EQ(mqtt_packet_parse_puback(pkt, 4, &pid), MQTT_PACKET_OK, "puback parsed");
    ASSERT_EQ(pid, 42, "packet id = 42");
}

static void test_roundtrip_publish(void)
{
    printf("%s\n", __func__);
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
    ASSERT(len > 0, "build ok");

    mqtt_publish_t pub_in = {0};
    ASSERT_EQ(mqtt_packet_parse_publish(buf, (size_t)len, &pub_in), MQTT_PACKET_OK, "parse ok");
    ASSERT(strncmp(pub_in.topic, "sensor/data", 11) == 0, "topic matches");
    ASSERT_EQ(pub_in.payload_len, 9, "payload len matches");
    ASSERT(memcmp(pub_in.payload, "temp=25.3", 9) == 0, "payload matches");
    ASSERT_EQ(pub_in.qos, 0, "qos matches");
}

static void test_null_params(void)
{
    printf("%s\n", __func__);
    ASSERT_EQ(mqtt_packet_build_connect(NULL, 128, NULL), MQTT_PACKET_ERR_PARAM, "connect null");
    ASSERT_EQ(mqtt_packet_build_publish(NULL, 128, NULL), MQTT_PACKET_ERR_PARAM, "publish null");
    ASSERT_EQ(mqtt_packet_build_subscribe(NULL, 128, 1, NULL, 0), MQTT_PACKET_ERR_PARAM, "sub null");
    ASSERT_EQ(mqtt_packet_build_pingreq(NULL, 2), MQTT_PACKET_ERR_SIZE, "ping null");

    mqtt_connack_t ca;
    ASSERT_EQ(mqtt_packet_parse_connack(NULL, 4, &ca), MQTT_PACKET_ERR_PARAM, "connack null buf");
    uint16_t pid;
    ASSERT_EQ(mqtt_packet_parse_puback(NULL, 4, &pid), MQTT_PACKET_ERR_PARAM, "puback null");
}

int main(void)
{
    printf("test_mqtt_packet\n");
    test_remaining_length_encode();
    test_remaining_length_decode();
    test_build_connect();
    test_build_connect_with_auth();
    test_build_connect_buf_too_small();
    test_build_publish_qos0();
    test_build_publish_qos1_retain();
    test_build_subscribe();
    test_build_pingreq();
    test_build_disconnect();
    test_get_type();
    test_parse_connack();
    test_parse_connack_rejected();
    test_parse_puback();
    test_roundtrip_publish();
    test_null_params();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
