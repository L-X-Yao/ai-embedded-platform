#include "test_harness.h"
#include "mqtt_client.h"
#include "mqtt_packet.h"
#include <string.h>

/* ─── mock transport ─────────────────────────────────────────── */

static uint8_t  g_sent[2048];
static uint16_t g_sent_len;
static uint32_t g_tick_ms;

static int mock_send(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (g_sent_len + len > sizeof(g_sent)) return -1;
    memcpy(&g_sent[g_sent_len], data, len);
    g_sent_len += len;
    return 0;
}

static uint32_t mock_tick(void *ctx)
{
    (void)ctx;
    return g_tick_ms;
}

static const mqtt_client_io_t g_io = {
    .send        = mock_send,
    .get_tick_ms = mock_tick,
    .ctx         = NULL,
};

/* ─── event capture ──────────────────────────────────────────── */

static mqtt_client_state_t g_evt_state;
static uint8_t             g_evt_reason;
static int                 g_evt_count;

static void evt_cb(void *ctx, mqtt_client_state_t s, uint8_t reason)
{
    (void)ctx;
    g_evt_state  = s;
    g_evt_reason = reason;
    g_evt_count++;
}

static char     g_msg_topic[128];
static uint8_t  g_msg_payload[512];
static uint16_t g_msg_len;
static uint8_t  g_msg_qos;
static int      g_msg_count;

static void msg_cb(void *ctx, const char *topic, uint16_t topic_len,
                   const uint8_t *payload, uint16_t len, uint8_t qos)
{
    (void)ctx;
    uint16_t copy = (topic_len < sizeof(g_msg_topic) - 1) ? topic_len : (uint16_t)(sizeof(g_msg_topic) - 1);
    memcpy(g_msg_topic, topic, copy);
    g_msg_topic[copy] = '\0';
    memcpy(g_msg_payload, payload, len);
    g_msg_len = len;
    g_msg_qos = qos;
    g_msg_count++;
}

static void reset_mocks(void)
{
    g_sent_len = 0; g_tick_ms = 0;
    g_evt_count = 0; g_evt_state = MQTT_CLIENT_DISCONNECTED; g_evt_reason = 0;
    g_msg_count = 0; g_msg_len = 0;
    memset(g_sent, 0, sizeof(g_sent));
    memset(g_msg_topic, 0, sizeof(g_msg_topic));
}

static mqtt_client_t g_c;

static void setup_connected(void)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = {
        .client_id = "test", .keepalive_s = 60,
        .clean_session = true, .connect_timeout_ms = 5000,
        .ack_timeout_ms = 3000,
    };
    mqtt_client_init(&g_c, &g_io, &cfg);
    mqtt_client_on_event(&g_c, evt_cb, NULL);
    mqtt_client_on_message(&g_c, msg_cb, NULL);
    mqtt_client_connect(&g_c);
    g_sent_len = 0;
    uint8_t connack[] = { 0x20, 0x02, 0x00, 0x00 };
    mqtt_client_feed(&g_c, connack, sizeof(connack));
}

/* ─── init tests ─────────────────────────────────────────────── */

TEST(test_init_null_params)
{
    mqtt_client_cfg_t cfg = { .client_id = "t" };
    ASSERT_EQ(mqtt_client_init(NULL, &g_io, &cfg), MQTT_CLIENT_ERR_PARAM);
    ASSERT_EQ(mqtt_client_init(&g_c, NULL, &cfg), MQTT_CLIENT_ERR_PARAM);
    ASSERT_EQ(mqtt_client_init(&g_c, &g_io, NULL), MQTT_CLIENT_ERR_PARAM);
    mqtt_client_cfg_t bad = { .client_id = NULL };
    ASSERT_EQ(mqtt_client_init(&g_c, &g_io, &bad), MQTT_CLIENT_ERR_PARAM);
}

TEST(test_init_defaults)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = { .client_id = "test", .keepalive_s = 30, .clean_session = true };
    ASSERT_EQ(mqtt_client_init(&g_c, &g_io, &cfg), MQTT_CLIENT_OK);
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_DISCONNECTED);
    ASSERT_EQ(g_c.cfg.connect_timeout_ms, 10000);
    ASSERT_EQ(g_c.cfg.ack_timeout_ms, 10000);
}

/* ─── connect tests ──────────────────────────────────────────── */

TEST(test_connect_sends_packet)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = { .client_id = "mydev", .keepalive_s = 60, .clean_session = true };
    mqtt_client_init(&g_c, &g_io, &cfg);
    ASSERT_EQ(mqtt_client_connect(&g_c), MQTT_CLIENT_OK);
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_CONNECTING);
    ASSERT_TRUE(g_sent_len > 10);
    ASSERT_EQ(g_sent[0] >> 4, MQTT_CONNECT);
}

TEST(test_connect_wrong_state)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = { .client_id = "t", .keepalive_s = 60, .clean_session = true };
    mqtt_client_init(&g_c, &g_io, &cfg);
    mqtt_client_connect(&g_c);
    ASSERT_EQ(mqtt_client_connect(&g_c), MQTT_CLIENT_ERR_STATE);
}

TEST(test_connect_with_credentials)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = {
        .client_id = "dev1", .username = "user", .password = "pass",
        .keepalive_s = 30, .clean_session = true,
    };
    mqtt_client_init(&g_c, &g_io, &cfg);
    ASSERT_EQ(mqtt_client_connect(&g_c), MQTT_CLIENT_OK);
    ASSERT_TRUE(g_sent_len > 20);
}

TEST(test_connack_success)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = { .client_id = "t", .keepalive_s = 60, .clean_session = true };
    mqtt_client_init(&g_c, &g_io, &cfg);
    mqtt_client_on_event(&g_c, evt_cb, NULL);
    mqtt_client_connect(&g_c);
    uint8_t connack[] = { 0x20, 0x02, 0x00, 0x00 };
    ASSERT_EQ(mqtt_client_feed(&g_c, connack, sizeof(connack)), MQTT_CLIENT_OK);
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_CONNECTED);
    ASSERT_EQ(g_evt_reason, 0);
}

TEST(test_connack_rejected)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = { .client_id = "t", .keepalive_s = 60, .clean_session = true };
    mqtt_client_init(&g_c, &g_io, &cfg);
    mqtt_client_on_event(&g_c, evt_cb, NULL);
    mqtt_client_connect(&g_c);
    uint8_t connack[] = { 0x20, 0x02, 0x00, 0x05 };
    mqtt_client_feed(&g_c, connack, sizeof(connack));
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_DISCONNECTED);
    ASSERT_EQ(g_evt_reason, 5);
}

TEST(test_connect_timeout)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = {
        .client_id = "t", .keepalive_s = 60,
        .clean_session = true, .connect_timeout_ms = 5000,
    };
    mqtt_client_init(&g_c, &g_io, &cfg);
    mqtt_client_on_event(&g_c, evt_cb, NULL);
    mqtt_client_connect(&g_c);
    g_tick_ms = 4999;
    mqtt_client_poll(&g_c);
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_CONNECTING);
    g_tick_ms = 5000;
    mqtt_client_poll(&g_c);
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_DISCONNECTED);
    ASSERT_EQ(g_evt_reason, 0xFF);
}

/* ─── publish tests ──────────────────────────────────────────── */

TEST(test_publish_qos0)
{
    setup_connected();
    g_sent_len = 0;
    ASSERT_EQ(mqtt_client_publish(&g_c, "t/a", (const uint8_t *)"hi", 2, 0), MQTT_CLIENT_OK);
    ASSERT_TRUE(g_sent_len > 0);
    ASSERT_EQ(g_sent[0] >> 4, MQTT_PUBLISH);
    ASSERT_FALSE(mqtt_client_pub_inflight(&g_c));
}

TEST(test_publish_qos1_and_puback)
{
    setup_connected();
    ASSERT_EQ(mqtt_client_publish(&g_c, "t/b", (const uint8_t *)"msg", 3, 1), MQTT_CLIENT_OK);
    ASSERT_TRUE(mqtt_client_pub_inflight(&g_c));
    uint16_t pkt_id = g_c.inflight_pub_id;
    uint8_t puback[] = { 0x40, 0x02, (uint8_t)(pkt_id >> 8), (uint8_t)(pkt_id & 0xFF) };
    mqtt_client_feed(&g_c, puback, sizeof(puback));
    ASSERT_FALSE(mqtt_client_pub_inflight(&g_c));
}

TEST(test_publish_qos1_busy)
{
    setup_connected();
    mqtt_client_publish(&g_c, "t/a", (const uint8_t *)"x", 1, 1);
    ASSERT_EQ(mqtt_client_publish(&g_c, "t/b", (const uint8_t *)"y", 1, 1), MQTT_CLIENT_ERR_BUSY);
}

TEST(test_publish_wrong_state)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = { .client_id = "t", .keepalive_s = 60, .clean_session = true };
    mqtt_client_init(&g_c, &g_io, &cfg);
    ASSERT_EQ(mqtt_client_publish(&g_c, "t", (const uint8_t *)"x", 1, 0), MQTT_CLIENT_ERR_STATE);
}

TEST(test_publish_null_topic)
{
    setup_connected();
    ASSERT_EQ(mqtt_client_publish(&g_c, NULL, (const uint8_t *)"x", 1, 0), MQTT_CLIENT_ERR_PARAM);
}

TEST(test_publish_qos2_rejected)
{
    setup_connected();
    ASSERT_EQ(mqtt_client_publish(&g_c, "t", (const uint8_t *)"x", 1, 2), MQTT_CLIENT_ERR_PARAM);
}

TEST(test_puback_timeout_clears_inflight)
{
    setup_connected();
    mqtt_client_publish(&g_c, "t", (const uint8_t *)"x", 1, 1);
    ASSERT_TRUE(mqtt_client_pub_inflight(&g_c));
    g_tick_ms = 3000;
    mqtt_client_poll(&g_c);
    ASSERT_FALSE(mqtt_client_pub_inflight(&g_c));
}

/* ─── subscribe tests ────────────────────────────────────────── */

TEST(test_subscribe_and_suback)
{
    setup_connected();
    g_sent_len = 0;
    ASSERT_EQ(mqtt_client_subscribe(&g_c, "s/topic", 1), MQTT_CLIENT_OK);
    ASSERT_TRUE(g_sent_len > 0);
    uint16_t sub_id = g_c.inflight_sub_id;
    uint8_t suback[] = { 0x90, 0x03, (uint8_t)(sub_id >> 8), (uint8_t)(sub_id & 0xFF), 0x01 };
    mqtt_client_feed(&g_c, suback, sizeof(suback));
    ASSERT_EQ(g_c.inflight_sub_id, 0);
}

TEST(test_subscribe_busy)
{
    setup_connected();
    mqtt_client_subscribe(&g_c, "s/1", 0);
    ASSERT_EQ(mqtt_client_subscribe(&g_c, "s/2", 0), MQTT_CLIENT_ERR_BUSY);
}

/* ─── incoming publish tests ─────────────────────────────────── */

TEST(test_incoming_publish_qos0)
{
    setup_connected();
    uint8_t pkt[] = {
        0x30, 0x0C,
        0x00, 0x05, 't', '/', 'm', 's', 'g',
        'h', 'e', 'l', 'l', 'o',
    };
    mqtt_client_feed(&g_c, pkt, sizeof(pkt));
    ASSERT_EQ(g_msg_count, 1);
    ASSERT_EQ(g_msg_qos, 0);
    ASSERT_EQ(g_msg_len, 5);
    ASSERT_MEMEQ(g_msg_payload, "hello");
}

TEST(test_incoming_publish_qos1_sends_puback)
{
    setup_connected();
    g_sent_len = 0;
    uint8_t pkt[] = {
        0x32, 0x0E,
        0x00, 0x05, 't', '/', 'm', 's', 'g',
        0x00, 0x07,
        'h', 'e', 'l', 'l', 'o',
    };
    mqtt_client_feed(&g_c, pkt, sizeof(pkt));
    ASSERT_EQ(g_msg_count, 1);
    ASSERT_TRUE(g_sent_len >= 4);
    ASSERT_EQ(g_sent[0], 0x40);
    ASSERT_EQ(g_sent[3], 0x07);
}

/* ─── keepalive tests ────────────────────────────────────────── */

TEST(test_keepalive_pingreq)
{
    setup_connected();
    g_sent_len = 0;
    g_tick_ms = 45000;
    mqtt_client_poll(&g_c);
    ASSERT_TRUE(g_sent_len >= 2);
    ASSERT_EQ(g_sent[0], 0xC0);
}

TEST(test_keepalive_timeout_disconnects)
{
    setup_connected();
    g_tick_ms = 90000;
    mqtt_client_poll(&g_c);
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_DISCONNECTED);
    ASSERT_EQ(g_evt_reason, 0xFE);
}

TEST(test_pingresp_resets_rx_timer)
{
    setup_connected();
    g_tick_ms = 45000;
    mqtt_client_poll(&g_c);
    uint8_t pingresp[] = { 0xD0, 0x00 };
    mqtt_client_feed(&g_c, pingresp, sizeof(pingresp));
    g_tick_ms = 89000;
    mqtt_client_poll(&g_c);
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_CONNECTED);
}

/* ─── disconnect tests ───────────────────────────────────────── */

TEST(test_disconnect)
{
    setup_connected();
    g_sent_len = 0;
    ASSERT_EQ(mqtt_client_disconnect(&g_c), MQTT_CLIENT_OK);
    ASSERT_EQ(mqtt_client_get_state(&g_c), MQTT_CLIENT_DISCONNECTED);
    ASSERT_TRUE(g_sent_len >= 2);
    ASSERT_EQ(g_sent[0], 0xE0);
}

/* ─── feed edge cases ────────────────────────────────────────── */

TEST(test_feed_partial_packet)
{
    setup_connected();
    uint8_t pkt[] = {
        0x30, 0x08,
        0x00, 0x03, 'a', '/', 'b',
        'X', 'Y', 'Z',
    };
    mqtt_client_feed(&g_c, pkt, 4);
    ASSERT_EQ(g_msg_count, 0);
    mqtt_client_feed(&g_c, &pkt[4], sizeof(pkt) - 4);
    ASSERT_EQ(g_msg_count, 1);
}

TEST(test_feed_when_disconnected)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = { .client_id = "t", .keepalive_s = 60, .clean_session = true };
    mqtt_client_init(&g_c, &g_io, &cfg);
    uint8_t data[] = { 0x20, 0x02, 0x00, 0x00 };
    ASSERT_EQ(mqtt_client_feed(&g_c, data, sizeof(data)), MQTT_CLIENT_ERR_STATE);
}

/* ─── utility tests ──────────────────────────────────────────── */

TEST(test_next_id_wraps)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = { .client_id = "t", .keepalive_s = 60, .clean_session = true };
    mqtt_client_init(&g_c, &g_io, &cfg);
    g_c.next_packet_id = 0xFFFF;
    ASSERT_EQ(mqtt_client_next_id(&g_c), 0xFFFF);
    ASSERT_EQ(mqtt_client_next_id(&g_c), 1);
}

TEST(test_get_state_null)
{
    ASSERT_EQ(mqtt_client_get_state(NULL), MQTT_CLIENT_DISCONNECTED);
}

TEST(test_pub_inflight_null)
{
    ASSERT_FALSE(mqtt_client_pub_inflight(NULL));
}

TEST(test_next_id_null)
{
    ASSERT_EQ(mqtt_client_next_id(NULL), 0);
}

/* ─── error code coverage ─────────────────────────────────────── */

static int mock_send_fail(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)data; (void)len;
    return -1;
}

TEST(test_connect_send_fail_returns_err_io)
{
    reset_mocks();
    mqtt_client_io_t fail_io = { .send = mock_send_fail, .get_tick_ms = mock_tick, .ctx = NULL };
    mqtt_client_cfg_t cfg = { .client_id = "t", .keepalive_s = 60, .clean_session = true };
    mqtt_client_init(&g_c, &fail_io, &cfg);
    ASSERT_EQ(mqtt_client_connect(&g_c), MQTT_CLIENT_ERR_IO);
}

TEST(test_publish_send_fail_returns_err_io)
{
    setup_connected();
    mqtt_client_io_t fail_io = { .send = mock_send_fail, .get_tick_ms = mock_tick, .ctx = NULL };
    g_c.io = &fail_io;
    ASSERT_EQ(mqtt_client_publish(&g_c, "t", (const uint8_t *)"x", 1, 0), MQTT_CLIENT_ERR_IO);
}

TEST(test_feed_bad_remaining_length_returns_err_protocol)
{
    setup_connected();
    uint8_t bad[] = { 0x30, 0x80, 0x80, 0x80, 0x80 };
    ASSERT_EQ(mqtt_client_feed(&g_c, bad, sizeof(bad)), MQTT_CLIENT_ERR_PROTOCOL);
}

TEST(test_feed_overflow_returns_err_size)
{
    setup_connected();
    g_c.rx_len = MQTT_CLIENT_RX_BUF_SIZE;
    uint8_t one = 0x00;
    ASSERT_EQ(mqtt_client_feed(&g_c, &one, 1), MQTT_CLIENT_ERR_SIZE);
}

TEST_MAIN("mqtt_client")
    RUN(test_init_null_params);
    RUN(test_init_defaults);
    RUN(test_connect_sends_packet);
    RUN(test_connect_wrong_state);
    RUN(test_connect_with_credentials);
    RUN(test_connack_success);
    RUN(test_connack_rejected);
    RUN(test_connect_timeout);
    RUN(test_publish_qos0);
    RUN(test_publish_qos1_and_puback);
    RUN(test_publish_qos1_busy);
    RUN(test_publish_wrong_state);
    RUN(test_publish_null_topic);
    RUN(test_publish_qos2_rejected);
    RUN(test_puback_timeout_clears_inflight);
    RUN(test_subscribe_and_suback);
    RUN(test_subscribe_busy);
    RUN(test_incoming_publish_qos0);
    RUN(test_incoming_publish_qos1_sends_puback);
    RUN(test_keepalive_pingreq);
    RUN(test_keepalive_timeout_disconnects);
    RUN(test_pingresp_resets_rx_timer);
    RUN(test_disconnect);
    RUN(test_feed_partial_packet);
    RUN(test_feed_when_disconnected);
    RUN(test_next_id_wraps);
    RUN(test_get_state_null);
    RUN(test_pub_inflight_null);
    RUN(test_next_id_null);
    RUN(test_connect_send_fail_returns_err_io);
    RUN(test_publish_send_fail_returns_err_io);
    RUN(test_feed_bad_remaining_length_returns_err_protocol);
    RUN(test_feed_overflow_returns_err_size);
TEST_MAIN_END()
