// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mqtt5_packet.h"
#include <string.h>

/* ---- VBI encode/decode ------------------------------------------------- */

TEST(test_vbi_encode_single_byte)
{
    uint8_t buf[4];
    ASSERT_EQ(mqtt5_encode_vbi(buf, 0), 1);
    ASSERT_EQ(buf[0], 0x00);
    ASSERT_EQ(mqtt5_encode_vbi(buf, 127), 1);
    ASSERT_EQ(buf[0], 0x7F);
}

TEST(test_vbi_encode_multi_byte)
{
    uint8_t buf[4];
    /* 128 = 0x00 0x01 */
    ASSERT_EQ(mqtt5_encode_vbi(buf, 128), 2);
    ASSERT_EQ(buf[0], 0x80);
    ASSERT_EQ(buf[1], 0x01);
    /* 16383 = 0xFF 0x7F */
    ASSERT_EQ(mqtt5_encode_vbi(buf, 16383), 2);
    ASSERT_EQ(buf[0], 0xFF);
    ASSERT_EQ(buf[1], 0x7F);
    /* 268435455 (max) = 4 bytes */
    ASSERT_EQ(mqtt5_encode_vbi(buf, 268435455u), 4);
}

TEST(test_vbi_encode_overflow)
{
    uint8_t buf[4];
    ASSERT_EQ(mqtt5_encode_vbi(buf, 268435456u), MQTT5_ERR_OVERFLOW);
}

TEST(test_vbi_encode_null)
{
    ASSERT_EQ(mqtt5_encode_vbi(NULL, 0), MQTT5_ERR_PARAM);
}

TEST(test_vbi_decode_single_byte)
{
    uint8_t data[] = {0x7F};
    uint32_t value;
    uint8_t used;
    ASSERT_EQ(mqtt5_decode_vbi(data, 1, &value, &used), MQTT5_OK);
    ASSERT_EQ(value, 127);
    ASSERT_EQ(used, 1);
}

TEST(test_vbi_decode_multi_byte)
{
    uint8_t data[] = {0x80, 0x01};
    uint32_t value;
    uint8_t used;
    ASSERT_EQ(mqtt5_decode_vbi(data, 2, &value, &used), MQTT5_OK);
    ASSERT_EQ(value, 128);
    ASSERT_EQ(used, 2);
}

TEST(test_vbi_decode_null_params)
{
    uint8_t data[] = {0x00};
    uint32_t value;
    uint8_t used;
    ASSERT_EQ(mqtt5_decode_vbi(NULL, 1, &value, &used), MQTT5_ERR_PARAM);
    ASSERT_EQ(mqtt5_decode_vbi(data, 1, NULL, &used), MQTT5_ERR_PARAM);
    ASSERT_EQ(mqtt5_decode_vbi(data, 1, &value, NULL), MQTT5_ERR_PARAM);
}

TEST(test_vbi_decode_short_buf)
{
    uint8_t data[] = {0x80};  /* continuation bit set, but no next byte */
    uint32_t value;
    uint8_t used;
    ASSERT_EQ(mqtt5_decode_vbi(data, 1, &value, &used), MQTT5_ERR_SIZE);
}

/* ---- VBI roundtrip ----------------------------------------------------- */

TEST(test_vbi_roundtrip)
{
    uint32_t test_values[] = {0, 1, 127, 128, 16383, 16384, 2097151, 268435455};
    for (int i = 0; i < 8; i++) {
        uint8_t buf[4];
        int enc_len = mqtt5_encode_vbi(buf, test_values[i]);
        ASSERT_TRUE(enc_len > 0);
        uint32_t decoded;
        uint8_t used;
        ASSERT_EQ(mqtt5_decode_vbi(buf, (uint16_t)enc_len, &decoded, &used), MQTT5_OK);
        ASSERT_EQ(decoded, test_values[i]);
        ASSERT_EQ(used, enc_len);
    }
}

/* ---- CONNECT ----------------------------------------------------------- */

TEST(test_build_connect_basic)
{
    uint8_t buf[128];
    mqtt5_connect_t cfg = {
        .keep_alive = 60,
        .client_id = "test",
        .clean_start = true,
        .session_expiry = 0,
    };
    int n = mqtt5_build_connect(buf, sizeof(buf), &cfg);
    ASSERT_TRUE(n > 0);

    /* fixed header: CONNECT */
    ASSERT_EQ(buf[0], 0x10);

    /* protocol name "MQTT" at known offset (after remaining length VBI) */
    /* remaining length is small, so 1-byte VBI at buf[1] */
    ASSERT_EQ(buf[1], n - 2);
    /* "MQTT" at buf[4..7] (len=0x0004, then 'M','Q','T','T') */
    ASSERT_EQ(buf[2], 0x00);
    ASSERT_EQ(buf[3], 0x04);
    ASSERT_MEMEQ(buf + 4, "MQTT");

    /* version = 5 */
    ASSERT_EQ(buf[8], 0x05);

    /* flags: clean_start = 0x02 */
    ASSERT_EQ(buf[9], 0x02);
}

TEST(test_build_connect_with_session_expiry)
{
    uint8_t buf[128];
    mqtt5_connect_t cfg = {
        .keep_alive = 30,
        .client_id = "dev1",
        .clean_start = false,
        .session_expiry = 3600,
    };
    int n = mqtt5_build_connect(buf, sizeof(buf), &cfg);
    ASSERT_TRUE(n > 0);

    /* version = 5 */
    ASSERT_EQ(buf[8], 0x05);

    /* flags: no clean_start, no user/pass */
    ASSERT_EQ(buf[9], 0x00);

    /* after keep_alive (2 bytes at 10-11), properties VBI then property */
    /* properties length = 5 (1 byte VBI = 0x05) */
    ASSERT_EQ(buf[12], 0x05);

    /* property id = 0x11 (session expiry) */
    ASSERT_EQ(buf[13], MQTT5_PROP_SESSION_EXPIRY);

    /* value = 3600 = 0x00000E10 */
    ASSERT_EQ(buf[14], 0x00);
    ASSERT_EQ(buf[15], 0x00);
    ASSERT_EQ(buf[16], 0x0E);
    ASSERT_EQ(buf[17], 0x10);
}

TEST(test_build_connect_with_credentials)
{
    uint8_t buf[128];
    mqtt5_connect_t cfg = {
        .keep_alive = 60,
        .client_id = "c",
        .username = "user",
        .password = "pass",
        .clean_start = true,
    };
    int n = mqtt5_build_connect(buf, sizeof(buf), &cfg);
    ASSERT_TRUE(n > 0);

    /* flags: clean_start(0x02) + username(0x80) + password(0x40) = 0xC2 */
    ASSERT_EQ(buf[9], 0xC2);
}

TEST(test_build_connect_null_params)
{
    uint8_t buf[128];
    mqtt5_connect_t cfg = { .client_id = "c" };
    ASSERT_EQ(mqtt5_build_connect(NULL, 128, &cfg), MQTT5_ERR_PARAM);
    ASSERT_EQ(mqtt5_build_connect(buf, 128, NULL), MQTT5_ERR_PARAM);

    mqtt5_connect_t bad = { .client_id = NULL };
    ASSERT_EQ(mqtt5_build_connect(buf, 128, &bad), MQTT5_ERR_PARAM);
}

TEST(test_build_connect_buffer_overflow)
{
    uint8_t buf[4];
    mqtt5_connect_t cfg = {
        .keep_alive = 60,
        .client_id = "test_client",
        .clean_start = true,
    };
    ASSERT_EQ(mqtt5_build_connect(buf, sizeof(buf), &cfg), MQTT5_ERR_SIZE);
}

/* ---- CONNACK parse ----------------------------------------------------- */

TEST(test_parse_connack_basic)
{
    /* Build a minimal CONNACK: type=0x20, remaining=3, ack_flags=0, reason=0, props_len=0 */
    uint8_t pkt[] = {0x20, 0x03, 0x00, 0x00, 0x00};
    mqtt5_connack_t ack;
    ASSERT_EQ(mqtt5_parse_connack(pkt, sizeof(pkt), &ack), MQTT5_OK);
    ASSERT_FALSE(ack.session_present);
    ASSERT_EQ(ack.reason_code, 0);
    ASSERT_EQ(ack.session_expiry, 0);
    ASSERT_EQ(ack.receive_max, 65535);
    ASSERT_EQ(ack.max_qos, 2);
}

TEST(test_parse_connack_with_properties)
{
    /* CONNACK with session_expiry=7200, receive_max=100, max_qos=1 */
    uint8_t pkt[] = {
        0x20,   /* CONNACK */
        0x0E,   /* remaining length = 14 */
        0x01,   /* ack flags: session_present=1 */
        0x00,   /* reason code: success */
        0x0A,   /* properties length = 10 */
        MQTT5_PROP_SESSION_EXPIRY, 0x00, 0x00, 0x1C, 0x20, /* 7200 */
        MQTT5_PROP_RECEIVE_MAX, 0x00, 0x64,                 /* 100 */
        MQTT5_PROP_MAX_QOS, 0x01,                            /* QoS 1 */
    };
    mqtt5_connack_t ack;
    ASSERT_EQ(mqtt5_parse_connack(pkt, sizeof(pkt), &ack), MQTT5_OK);
    ASSERT_TRUE(ack.session_present);
    ASSERT_EQ(ack.reason_code, 0);
    ASSERT_EQ(ack.session_expiry, 7200);
    ASSERT_EQ(ack.receive_max, 100);
    ASSERT_EQ(ack.max_qos, 1);
}

TEST(test_parse_connack_reason_code)
{
    /* CONNACK with reason code 0x86 (topic name invalid) */
    uint8_t pkt[] = {0x20, 0x03, 0x00, 0x86, 0x00};
    mqtt5_connack_t ack;
    ASSERT_EQ(mqtt5_parse_connack(pkt, sizeof(pkt), &ack), MQTT5_OK);
    ASSERT_EQ(ack.reason_code, 0x86);
}

TEST(test_parse_connack_null_params)
{
    uint8_t pkt[] = {0x20, 0x03, 0x00, 0x00, 0x00};
    mqtt5_connack_t ack;
    ASSERT_EQ(mqtt5_parse_connack(NULL, 5, &ack), MQTT5_ERR_PARAM);
    ASSERT_EQ(mqtt5_parse_connack(pkt, 5, NULL), MQTT5_ERR_PARAM);
}

/* ---- PUBLISH build + parse roundtrip ----------------------------------- */

TEST(test_build_publish_qos0)
{
    uint8_t buf[128];
    const uint8_t payload[] = "hello";
    mqtt5_publish_t pub = {
        .topic = "t/1",
        .payload = payload,
        .payload_len = 5,
        .qos = 0,
        .retain = false,
    };
    int n = mqtt5_build_publish(buf, sizeof(buf), &pub);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(buf[0] & 0xF0, 0x30);  /* PUBLISH */
    ASSERT_EQ(buf[0] & 0x01, 0x00);  /* no retain */
}

TEST(test_build_publish_qos1_retain)
{
    uint8_t buf[128];
    const uint8_t payload[] = "data";
    mqtt5_publish_t pub = {
        .topic = "sensor/temp",
        .payload = payload,
        .payload_len = 4,
        .packet_id = 42,
        .qos = 1,
        .retain = true,
    };
    int n = mqtt5_build_publish(buf, sizeof(buf), &pub);
    ASSERT_TRUE(n > 0);
    /* byte0: PUBLISH(0x30) | QoS1(0x02) | retain(0x01) = 0x33 */
    ASSERT_EQ(buf[0], 0x33);
}

TEST(test_publish_roundtrip_with_expiry)
{
    uint8_t buf[256];
    const uint8_t payload[] = {0xDE, 0xAD};
    mqtt5_publish_t pub_out = {
        .topic = "a/b",
        .payload = payload,
        .payload_len = 2,
        .packet_id = 100,
        .qos = 1,
        .retain = false,
        .message_expiry = 300,
    };
    int n = mqtt5_build_publish(buf, sizeof(buf), &pub_out);
    ASSERT_TRUE(n > 0);

    mqtt5_publish_t pub_in;
    memset(&pub_in, 0, sizeof(pub_in));
    ASSERT_EQ(mqtt5_parse_publish(buf, (uint16_t)n, &pub_in), MQTT5_OK);
    ASSERT_EQ(pub_in.qos, 1);
    ASSERT_FALSE(pub_in.retain);
    ASSERT_EQ(pub_in.packet_id, 100);
    ASSERT_EQ(pub_in.message_expiry, 300);
    ASSERT_EQ(pub_in.payload_len, 2);
    ASSERT_EQ(pub_in.payload[0], 0xDE);
    ASSERT_EQ(pub_in.payload[1], 0xAD);
    /* topic is pointer into buf — compare content */
    ASSERT_TRUE(memcmp(pub_in.topic, "a/b", 3) == 0);
}

TEST(test_build_publish_null_params)
{
    uint8_t buf[64];
    mqtt5_publish_t pub = { .topic = "t" };
    ASSERT_EQ(mqtt5_build_publish(NULL, 64, &pub), MQTT5_ERR_PARAM);
    ASSERT_EQ(mqtt5_build_publish(buf, 64, NULL), MQTT5_ERR_PARAM);

    mqtt5_publish_t bad = { .topic = NULL };
    ASSERT_EQ(mqtt5_build_publish(buf, 64, &bad), MQTT5_ERR_PARAM);
}

TEST(test_build_publish_qos_invalid)
{
    uint8_t buf[64];
    mqtt5_publish_t pub = { .topic = "t", .qos = 3 };
    ASSERT_EQ(mqtt5_build_publish(buf, sizeof(buf), &pub), MQTT5_ERR_PARAM);
}

TEST(test_build_publish_buffer_overflow)
{
    uint8_t buf[4];
    mqtt5_publish_t pub = {
        .topic = "long/topic/name",
        .payload = (const uint8_t *)"x",
        .payload_len = 1,
    };
    ASSERT_EQ(mqtt5_build_publish(buf, sizeof(buf), &pub), MQTT5_ERR_SIZE);
}

/* ---- SUBSCRIBE --------------------------------------------------------- */

TEST(test_build_subscribe)
{
    uint8_t buf[128];
    int n = mqtt5_build_subscribe(buf, sizeof(buf), 1, "sensor/#", 1);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(buf[0], 0x82);  /* SUBSCRIBE */
}

TEST(test_build_subscribe_qos_levels)
{
    uint8_t buf[128];
    /* QoS 0 */
    int n0 = mqtt5_build_subscribe(buf, sizeof(buf), 10, "a", 0);
    ASSERT_TRUE(n0 > 0);
    ASSERT_EQ(buf[n0 - 1], 0x00);  /* sub options = QoS 0 */

    /* QoS 2 */
    int n2 = mqtt5_build_subscribe(buf, sizeof(buf), 11, "b", 2);
    ASSERT_TRUE(n2 > 0);
    ASSERT_EQ(buf[n2 - 1], 0x02);  /* sub options = QoS 2 */
}

TEST(test_build_subscribe_null_params)
{
    uint8_t buf[64];
    ASSERT_EQ(mqtt5_build_subscribe(NULL, 64, 1, "t", 0), MQTT5_ERR_PARAM);
    ASSERT_EQ(mqtt5_build_subscribe(buf, 64, 1, NULL, 0), MQTT5_ERR_PARAM);
}

TEST(test_build_subscribe_overflow)
{
    uint8_t buf[4];
    ASSERT_EQ(mqtt5_build_subscribe(buf, sizeof(buf), 1, "topic", 0), MQTT5_ERR_SIZE);
}

/* ---- DISCONNECT -------------------------------------------------------- */

TEST(test_build_disconnect_normal)
{
    uint8_t buf[16];
    int n = mqtt5_build_disconnect(buf, sizeof(buf), 0x00);
    ASSERT_EQ(n, 2);
    ASSERT_EQ(buf[0], 0xE0);
    ASSERT_EQ(buf[1], 0x00);
}

TEST(test_build_disconnect_with_reason)
{
    uint8_t buf[16];
    /* 0x04 = disconnect with will message */
    int n = mqtt5_build_disconnect(buf, sizeof(buf), 0x04);
    ASSERT_EQ(n, 4);
    ASSERT_EQ(buf[0], 0xE0);
    ASSERT_EQ(buf[2], 0x04);
    ASSERT_EQ(buf[3], 0x00);  /* no properties */
}

TEST(test_build_disconnect_null)
{
    ASSERT_EQ(mqtt5_build_disconnect(NULL, 16, 0), MQTT5_ERR_PARAM);
}

TEST(test_build_disconnect_overflow)
{
    uint8_t buf[1];
    ASSERT_EQ(mqtt5_build_disconnect(buf, sizeof(buf), 0x00), MQTT5_ERR_SIZE);
}

/* ---- parse_publish edge cases ------------------------------------------ */

TEST(test_parse_publish_null)
{
    uint8_t data[] = {0x30, 0x03, 0x00, 0x01, 0x74};
    mqtt5_publish_t pub;
    ASSERT_EQ(mqtt5_parse_publish(NULL, 5, &pub), MQTT5_ERR_PARAM);
    ASSERT_EQ(mqtt5_parse_publish(data, 5, NULL), MQTT5_ERR_PARAM);
}

TEST(test_parse_publish_short)
{
    uint8_t data[] = {0x30, 0x00};
    mqtt5_publish_t pub;
    ASSERT_EQ(mqtt5_parse_publish(data, 2, &pub), MQTT5_ERR_SIZE);
}

TEST(test_parse_publish_wrong_type)
{
    /* byte 0 = 0x20 = CONNACK, not PUBLISH */
    uint8_t data[] = {0x20, 0x03, 0x00, 0x01, 0x74};
    mqtt5_publish_t pub;
    ASSERT_EQ(mqtt5_parse_publish(data, sizeof(data), &pub), MQTT5_ERR_PROTOCOL);
}

TEST(test_build_connect_remaining_length_counts_the_frame)
{
    /* MQTT 5.0 section 3.1: variable header 10, property length 1,
     * client id 2 + 3. The whole frame is 2 + 16 bytes. */
    uint8_t buf[18];
    mqtt5_connect_t cfg = {
        .keep_alive = 60,
        .client_id = "abc",
        .clean_start = true,
    };

    ASSERT_EQ(mqtt5_build_connect(buf, sizeof(buf), &cfg), 18);
    ASSERT_EQ(buf[1], 16);
    ASSERT_EQ(mqtt5_build_connect(buf, sizeof(buf) - 1, &cfg), MQTT5_ERR_SIZE);
}

TEST(test_build_connect_remaining_length_with_properties)
{
    /* Session expiry adds an identifier and four value bytes. */
    uint8_t buf[23];
    mqtt5_connect_t cfg = {
        .client_id = "abc",
        .session_expiry = 3600,
    };

    ASSERT_EQ(mqtt5_build_connect(buf, sizeof(buf), &cfg), 23);
    ASSERT_EQ(buf[1], 21);
}

TEST(test_build_publish_length_without_payload)
{
    /* A length with no bytes would announce a payload the frame does not hold. */
    uint8_t buf[32];
    mqtt5_publish_t pub = { .topic = "t", .payload = NULL, .payload_len = 4 };

    ASSERT_EQ(mqtt5_build_publish(buf, sizeof(buf), &pub), MQTT5_ERR_PARAM);

    pub.payload_len = 0;
    ASSERT_TRUE(mqtt5_build_publish(buf, sizeof(buf), &pub) > 0);
}

/* MQTT 5.0 section 3.3.2.1 and 1.5.4: the topic is a length-prefixed string with
   no terminator, so the byte after it is whatever the packet holds next */
TEST(test_parse_publish_reports_the_topic_length_at_qos_0)
{
    static const uint8_t frame[] = {
        0x30, 0x08, 0x00, 0x03, 'a', '/', 'b', 0x00, 'h', 'i',
    };
    mqtt5_publish_t pub;
    ASSERT_EQ(mqtt5_parse_publish(frame, sizeof(frame), &pub), MQTT5_OK);
    ASSERT_EQ(pub.topic_len, 3);
    ASSERT_EQ(memcmp(pub.topic, "a/b", 3), 0);
    ASSERT_EQ(pub.payload_len, 2);
}

TEST(test_parse_publish_reports_the_topic_length_at_qos_1)
{
    /* Packet id 0x0102 and an expiry of 60 follow the topic, with no zero byte
       right after it: strlen() would run on into them */
    static const uint8_t frame[] = {
        0x32, 0x0F, 0x00, 0x03, 'a', '/', 'b', 0x01, 0x02,
        0x05, 0x02, 0x00, 0x00, 0x00, 0x3C, 'h', 'i',
    };
    mqtt5_publish_t pub;
    ASSERT_EQ(mqtt5_parse_publish(frame, sizeof(frame), &pub), MQTT5_OK);
    ASSERT_EQ(pub.topic_len, 3);
    ASSERT_EQ(memcmp(pub.topic, "a/b", 3), 0);
    ASSERT_EQ(pub.packet_id, 0x0102);
    ASSERT_EQ(pub.message_expiry, 60U);
}

TEST(test_parse_publish_reports_a_topic_at_the_end_of_the_buffer)
{
    static const uint8_t frame[] = { 0x30, 0x05, 0x00, 0x03, 'a', '/', 'b' };
    mqtt5_publish_t pub;
    ASSERT_EQ(mqtt5_parse_publish(frame, sizeof(frame), &pub), MQTT5_OK);
    ASSERT_EQ(pub.topic_len, 3);
    ASSERT_EQ(pub.topic, (const char *)&frame[4]);
}

/* MQTT 5.0 section 3.3.2.3.4: with a Topic Alias the topic can be empty */
TEST(test_parse_publish_reports_an_empty_topic)
{
    static const uint8_t frame[] = {
        0x30, 0x06, 0x00, 0x00, 0x03, 0x23, 0x00, 0x01,
    };
    mqtt5_publish_t pub;
    memset(&pub, 0xFF, sizeof(pub));
    ASSERT_EQ(mqtt5_parse_publish(frame, sizeof(frame), &pub), MQTT5_OK);
    ASSERT_EQ(pub.topic_len, 0);
}

TEST(test_parse_publish_reports_a_topic_past_one_byte_of_length)
{
    char topic[301];
    memset(topic, 't', 300);
    topic[300] = '\0';
    mqtt5_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.topic = topic;

    uint8_t frame[400];
    int n = mqtt5_build_publish(frame, sizeof(frame), &pub);
    ASSERT_TRUE(n > 0);

    mqtt5_publish_t got;
    ASSERT_EQ(mqtt5_parse_publish(frame, (uint16_t)n, &got), MQTT5_OK);
    ASSERT_EQ(got.topic_len, 300);
    ASSERT_EQ(memcmp(got.topic, topic, 300), 0);
}

TEST(test_build_publish_goes_by_the_string_not_topic_len)
{
    mqtt5_publish_t pub;
    memset(&pub, 0, sizeof(pub));
    pub.topic = "a/b";
    pub.topic_len = 1;

    uint8_t frame[32];
    int n = mqtt5_build_publish(frame, sizeof(frame), &pub);
    ASSERT_TRUE(n > 0);

    mqtt5_publish_t got;
    ASSERT_EQ(mqtt5_parse_publish(frame, (uint16_t)n, &got), MQTT5_OK);
    ASSERT_EQ(got.topic_len, 3);
}

TEST(test_parse_publish_refuses_both_qos_bits)
{
    const uint8_t frame[] = { 0x36, 0x06, 0x00, 0x01, 't', 0x00, 0x01, 0x00 };
    mqtt5_publish_t pub;

    ASSERT_EQ(mqtt5_parse_publish(frame, sizeof(frame), &pub), MQTT5_ERR_PROTOCOL);
}

/* --- properties the codec does not store (MQTT 5.0 section 2.2.2.2) --- */

static uint16_t frame_connack(uint8_t *out, const uint8_t *props, uint8_t n)
{
    out[0] = 0x20;
    out[1] = (uint8_t)(3 + n);  /* flags, reason, property length, properties */
    out[2] = 0x00;
    out[3] = 0x00;
    out[4] = n;
    memcpy(&out[5], props, n);
    return (uint16_t)(5 + n);
}

/* QoS 0 PUBLISH to topic "t" with payload "hi". */
static uint16_t frame_publish(uint8_t *out, const uint8_t *props, uint8_t n)
{
    out[0] = 0x30;
    out[1] = (uint8_t)(3 + 1 + n + 2);
    out[2] = 0x00;
    out[3] = 0x01;
    out[4] = 't';
    out[5] = n;
    memcpy(&out[6], props, n);
    out[6 + n] = 'h';
    out[7 + n] = 'i';
    return (uint16_t)(8 + n);
}

static mqtt5_err_t connack_with(const uint8_t *props, uint8_t n)
{
    uint8_t frame[100];
    mqtt5_connack_t ack;
    uint16_t len = frame_connack(frame, props, n);
    return mqtt5_parse_connack(frame, len, &ack);
}

static mqtt5_err_t publish_with(const uint8_t *props, uint8_t n)
{
    uint8_t frame[100];
    mqtt5_publish_t pub;
    uint16_t len = frame_publish(frame, props, n);
    return mqtt5_parse_publish(frame, len, &pub);
}

TEST(test_parse_connack_from_a_default_broker)
{
    /* Reason 0 and Topic Alias Maximum 10. */
    const uint8_t frame[] = { 0x20, 0x06, 0x00, 0x00, 0x03, 0x22, 0x00, 0x0A };
    mqtt5_connack_t ack;

    ASSERT_EQ(mqtt5_parse_connack(frame, sizeof(frame), &ack), MQTT5_OK);
    ASSERT_EQ(ack.reason_code, 0);
    ASSERT_EQ(ack.session_expiry, 0u);
    ASSERT_EQ(ack.receive_max, 65535u);
    ASSERT_EQ(ack.max_qos, 2);
}

TEST(test_parse_connack_skips_every_property_it_does_not_store)
{
    static const uint8_t props[] = {
        0x12, 0x00, 0x02, 'i', 'd',     /* Assigned Client Identifier */
        0x13, 0x00, 0x3C,               /* Server Keep Alive */
        0x15, 0x00, 0x01, 'm',          /* Authentication Method */
        0x16, 0x00, 0x02, 0xAA, 0xBB,   /* Authentication Data */
        0x1A, 0x00, 0x01, 'r',          /* Response Information */
        0x1C, 0x00, 0x01, 's',          /* Server Reference */
        0x1F, 0x00, 0x01, 'e',          /* Reason String */
        0x22, 0x00, 0x0A,               /* Topic Alias Maximum */
        0x25, 0x01,                     /* Retain Available */
        0x26, 0x00, 0x01, 'k', 0x00, 0x01, 'v', /* User Property */
        0x27, 0x00, 0x00, 0x10, 0x00,   /* Maximum Packet Size */
        0x28, 0x01,                     /* Wildcard Subscription Available */
        0x29, 0x01,                     /* Subscription Identifier Available */
        0x2A, 0x00,                     /* Shared Subscription Available */
        0x24, 0x01,                     /* Maximum QoS */
        0x21, 0x00, 0x14,               /* Receive Maximum */
        0x11, 0x00, 0x00, 0x00, 0x1E,   /* Session Expiry Interval */
    };
    uint8_t frame[100];
    mqtt5_connack_t ack;
    uint16_t len = frame_connack(frame, props, sizeof(props));

    ASSERT_EQ(mqtt5_parse_connack(frame, len, &ack), MQTT5_OK);
    /* These sit last, so one wrong skip would misread them. */
    ASSERT_EQ(ack.max_qos, 1);
    ASSERT_EQ(ack.receive_max, 20u);
    ASSERT_EQ(ack.session_expiry, 30u);
}

TEST(test_parse_publish_skips_every_property_it_does_not_store)
{
    static const uint8_t props[] = {
        0x01, 0x01,                     /* Payload Format Indicator */
        0x03, 0x00, 0x02, 'a', 'b',     /* Content Type */
        0x08, 0x00, 0x01, 'r',          /* Response Topic */
        0x09, 0x00, 0x02, 0x01, 0x02,   /* Correlation Data */
        0x0B, 0x80, 0x01,               /* Subscription Identifier 128 */
        0x23, 0x00, 0x05,               /* Topic Alias */
        0x26, 0x00, 0x01, 'k', 0x00, 0x01, 'v', /* User Property */
        0x02, 0x00, 0x00, 0x00, 0x3C,   /* Message Expiry Interval */
    };
    uint8_t frame[100];
    mqtt5_publish_t pub;
    uint16_t len = frame_publish(frame, props, sizeof(props));

    ASSERT_EQ(mqtt5_parse_publish(frame, len, &pub), MQTT5_OK);
    ASSERT_EQ(pub.message_expiry, 60u);
    ASSERT_EQ(pub.payload_len, 2);
    ASSERT_EQ(pub.payload[0], 'h');
    ASSERT_EQ(pub.payload[1], 'i');
}

TEST(test_properties_refuse_ids_the_spec_does_not_define)
{
    static const uint8_t undefined[] = { 0x00, 0x04, 0x14, 0x2B, 0x7F, 0xFF };

    for (size_t i = 0; i < sizeof(undefined); i++) {
        uint8_t prop[3] = { undefined[i], 0x00, 0x00 };
        ASSERT_EQ(connack_with(prop, 3), MQTT5_ERR_PROTOCOL);
        ASSERT_EQ(publish_with(prop, 3), MQTT5_ERR_PROTOCOL);
    }
}

TEST(test_properties_refuse_ids_of_other_packets)
{
    /* Section 2.2.2.2: an Identifier not valid for its packet type is a
     * Malformed Packet. */
    static const uint8_t in_connack[][5] = {
        { 0x02, 0x00, 0x00, 0x00, 0x01 },   /* Message Expiry Interval */
        { 0x18, 0x00, 0x00, 0x00, 0x01 },   /* Will Delay Interval */
    };
    static const uint8_t alias[3] = { 0x23, 0x00, 0x01 };
    static const uint8_t request[2] = { 0x17, 0x01 };
    static const uint8_t max_qos[2] = { 0x24, 0x01 };
    static const uint8_t keep_alive[3] = { 0x13, 0x00, 0x01 };

    ASSERT_EQ(connack_with(in_connack[0], 5), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(in_connack[1], 5), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(alias, 3), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(request, 2), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(publish_with(max_qos, 2), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(publish_with(keep_alive, 3), MQTT5_ERR_PROTOCOL);
}

TEST(test_skipped_property_cannot_overrun_the_block)
{
    static const uint8_t cut_byte[1] = { 0x25 };
    static const uint8_t cut_u16[2] = { 0x22, 0x00 };
    static const uint8_t cut_u32[4] = { 0x27, 0x00, 0x00, 0x10 };
    static const uint8_t no_len[2] = { 0x1F, 0x00 };
    static const uint8_t cut_str[4] = { 0x1F, 0x00, 0x05, 'a' };
    static const uint8_t no_value[4] = { 0x26, 0x00, 0x01, 'k' };
    static const uint8_t cut_value[7] = { 0x26, 0x00, 0x01, 'k', 0x00, 0x05, 'v' };
    static const uint8_t cut_vbi[2] = { 0x0B, 0x80 };
    static const uint8_t long_vbi[6] = { 0x0B, 0x80, 0x80, 0x80, 0x80, 0x01 };

    ASSERT_EQ(connack_with(cut_byte, 1), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(cut_u16, 2), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(cut_u32, 4), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(no_len, 2), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(cut_str, 4), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(no_value, 4), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(connack_with(cut_value, 7), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(publish_with(no_value, 4), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(publish_with(cut_vbi, 2), MQTT5_ERR_PROTOCOL);
    ASSERT_EQ(publish_with(long_vbi, 6), MQTT5_ERR_PROTOCOL);
}

TEST_MAIN("mqtt5_packet")
    /* VBI */
    RUN(test_vbi_encode_single_byte);
    RUN(test_vbi_encode_multi_byte);
    RUN(test_vbi_encode_overflow);
    RUN(test_vbi_encode_null);
    RUN(test_vbi_decode_single_byte);
    RUN(test_vbi_decode_multi_byte);
    RUN(test_vbi_decode_null_params);
    RUN(test_vbi_decode_short_buf);
    RUN(test_vbi_roundtrip);
    /* CONNECT */
    RUN(test_build_connect_basic);
    RUN(test_build_connect_with_session_expiry);
    RUN(test_build_connect_with_credentials);
    RUN(test_build_connect_null_params);
    RUN(test_build_connect_buffer_overflow);
    /* CONNACK */
    RUN(test_parse_connack_basic);
    RUN(test_parse_connack_with_properties);
    RUN(test_parse_connack_reason_code);
    RUN(test_parse_connack_null_params);
    /* PUBLISH */
    RUN(test_build_publish_qos0);
    RUN(test_build_publish_qos1_retain);
    RUN(test_publish_roundtrip_with_expiry);
    RUN(test_build_publish_null_params);
    RUN(test_build_publish_qos_invalid);
    RUN(test_build_publish_buffer_overflow);
    /* SUBSCRIBE */
    RUN(test_build_subscribe);
    RUN(test_build_subscribe_qos_levels);
    RUN(test_build_subscribe_null_params);
    RUN(test_build_subscribe_overflow);
    /* DISCONNECT */
    RUN(test_build_disconnect_normal);
    RUN(test_build_disconnect_with_reason);
    RUN(test_build_disconnect_null);
    RUN(test_build_disconnect_overflow);
    /* parse edge cases */
    RUN(test_parse_publish_null);
    RUN(test_parse_publish_short);
    RUN(test_parse_publish_wrong_type);
    RUN(test_build_connect_remaining_length_counts_the_frame);
    RUN(test_build_connect_remaining_length_with_properties);
    RUN(test_build_publish_length_without_payload);
    RUN(test_parse_publish_reports_the_topic_length_at_qos_0);
    RUN(test_parse_publish_reports_the_topic_length_at_qos_1);
    RUN(test_parse_publish_reports_a_topic_at_the_end_of_the_buffer);
    RUN(test_parse_publish_reports_an_empty_topic);
    RUN(test_parse_publish_reports_a_topic_past_one_byte_of_length);
    RUN(test_build_publish_goes_by_the_string_not_topic_len);
    RUN(test_parse_publish_refuses_both_qos_bits);
    /* properties the codec does not store */
    RUN(test_parse_connack_from_a_default_broker);
    RUN(test_parse_connack_skips_every_property_it_does_not_store);
    RUN(test_parse_publish_skips_every_property_it_does_not_store);
    RUN(test_properties_refuse_ids_the_spec_does_not_define);
    RUN(test_properties_refuse_ids_of_other_packets);
    RUN(test_skipped_property_cannot_overrun_the_block);
TEST_MAIN_END()
