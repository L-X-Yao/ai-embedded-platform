/*
 * prop_mqtt_client.c -- property-based tests for mqtt_client
 *
 * Build: gcc -std=c99 -I.. -I../../protocols/mqtt_packet -I../../../../tooling/harness
 *        prop_mqtt_client.c ../mqtt_client.c
 *        ../../protocols/mqtt_packet/mqtt_packet.c
 *        -o prop_mqtt_client
 */

#include "test_harness.h"
#include "property_test.h"
#include "../mqtt_client.h"
#include <string.h>

/* ========== mock transport ========== */

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

static void reset_mocks(void)
{
    g_sent_len = 0;
    g_tick_ms = 0;
    memset(g_sent, 0, sizeof(g_sent));
}

/* helper: initialize and connect a client */
static mqtt_client_t g_c;

static void setup_connected(void)
{
    reset_mocks();
    mqtt_client_cfg_t cfg = {
        .client_id = "prop", .keepalive_s = 60,
        .clean_session = true, .connect_timeout_ms = 5000,
        .ack_timeout_ms = 3000,
    };
    mqtt_client_init(&g_c, &g_io, &cfg);
    mqtt_client_connect(&g_c);
    g_sent_len = 0;
    /* feed CONNACK success */
    uint8_t connack[] = { 0x20, 0x02, 0x00, 0x00 };
    mqtt_client_feed(&g_c, connack, sizeof(connack));
}

/* --- Property 1: init/deinit symmetry -- double init is safe --- */

PROPERTY(double_init_safe, 5000) {
    (void)seed;
    reset_mocks();
    mqtt_client_t c;
    mqtt_client_cfg_t cfg = {
        .client_id = "test", .keepalive_s = 60, .clean_session = true,
    };
    mqtt_client_err_t rc1 = mqtt_client_init(&c, &g_io, &cfg);
    PROP_ASSERT_EQ(rc1, MQTT_CLIENT_OK);
    PROP_ASSERT_EQ(mqtt_client_get_state(&c), MQTT_CLIENT_DISCONNECTED);

    mqtt_client_err_t rc2 = mqtt_client_init(&c, &g_io, &cfg);
    PROP_ASSERT_EQ(rc2, MQTT_CLIENT_OK);
    PROP_ASSERT_EQ(mqtt_client_get_state(&c), MQTT_CLIENT_DISCONNECTED);
}

/* --- Property 2: next_id always returns 1..65535 and wraps --- */

PROPERTY(next_id_range, 10000) {
    reset_mocks();
    mqtt_client_t c;
    mqtt_client_cfg_t cfg = {
        .client_id = "t", .keepalive_s = 60, .clean_session = true,
    };
    mqtt_client_init(&c, &g_io, &cfg);

    /* set to random position */
    c.next_packet_id = ARBI_UINT16(seed);

    int i;
    for (i = 0; i < 10; i++) {
        uint16_t id = mqtt_client_next_id(&c);
        PROP_ASSERT(id >= 1);
        /* 0 is never returned */
    }
}

/* --- Property 3: publish in wrong state always returns ERR_STATE --- */

PROPERTY(publish_wrong_state_rejected, 10000) {
    (void)seed;
    reset_mocks();
    mqtt_client_t c;
    mqtt_client_cfg_t cfg = {
        .client_id = "t", .keepalive_s = 60, .clean_session = true,
    };
    mqtt_client_init(&c, &g_io, &cfg);

    /* DISCONNECTED state */
    uint8_t payload = ARBI_UINT8(seed);
    mqtt_client_err_t rc = mqtt_client_publish(
        &c, "topic", &payload, 1, 0);
    PROP_ASSERT_EQ(rc, MQTT_CLIENT_ERR_STATE);
}

/* --- Property 4: feed random bytes when connected does not crash --- */

PROPERTY(feed_random_no_crash, 5000) {
    setup_connected();

    uint8_t buf[64];
    size_t len = ARBI_SIZE(seed, 63) + 1;
    ARBI_FILL(seed, buf, len);

    /* may return error, but must not crash */
    mqtt_client_feed(&g_c, buf, (uint16_t)len);
    PROP_ASSERT(1);
}

/* --- Property 5: poll never crashes regardless of tick value --- */

PROPERTY(poll_no_crash, 10000) {
    setup_connected();

    g_tick_ms = ARBI_UINT32(seed);
    mqtt_client_poll(&g_c);

    /* state may have changed (timeout/keepalive) but no crash */
    mqtt_client_state_t st = mqtt_client_get_state(&g_c);
    PROP_ASSERT(st == MQTT_CLIENT_CONNECTED ||
                st == MQTT_CLIENT_DISCONNECTED ||
                st == MQTT_CLIENT_CONNECTING);
}

TEST_MAIN("mqtt_client properties") {
    RUN(double_init_safe);
    RUN(next_id_range);
    RUN(publish_wrong_state_rejected);
    RUN(feed_random_no_crash);
    RUN(poll_no_crash);
}
TEST_MAIN_END()
