#include "test_harness.h"
#include <string.h>
#include "air780eg.h"

/* ---------- mock transport ---------- */

static uint8_t g_tx_buf[2048];
static uint16_t g_tx_len;
static uint32_t g_tick_ms;

static void mock_send(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (g_tx_len + len <= sizeof(g_tx_buf)) {
        memcpy(&g_tx_buf[g_tx_len], data, len);
        g_tx_len += len;
    }
}

static uint32_t mock_get_tick(void *ctx)
{
    (void)ctx;
    return g_tick_ms;
}

static const air780eg_io_t g_mock_io = {
    .send        = mock_send,
    .get_tick_ms = mock_get_tick,
    .ctx         = NULL,
};

static void mock_reset(void)
{
    g_tx_len = 0;
    g_tick_ms = 0;
    memset(g_tx_buf, 0, sizeof(g_tx_buf));
}

/* Feed a complete line (adds \r\n) */
static void feed_line(air780eg_t *dev, const char *line)
{
    for (const char *p = line; *p; p++) {
        air780eg_feed(dev, (uint8_t)*p);
    }
    air780eg_feed(dev, '\r');
    air780eg_feed(dev, '\n');
}

/* ---------- callback state ---------- */

static int g_mqtt_rx_count;
static char g_mqtt_rx_topic[128];
static char g_mqtt_rx_payload[256];
static uint16_t g_mqtt_rx_payload_len;

static void on_mqtt_rx(void *user, const char *topic, const uint8_t *payload, uint16_t len)
{
    (void)user;
    g_mqtt_rx_count++;
    strncpy(g_mqtt_rx_topic, topic, sizeof(g_mqtt_rx_topic) - 1);
    if (len < sizeof(g_mqtt_rx_payload)) {
        memcpy(g_mqtt_rx_payload, payload, len);
        g_mqtt_rx_payload[len] = '\0';
    }
    g_mqtt_rx_payload_len = len;
}

static int g_disconnect_count;
static void on_disconnect(void *user) { (void)user; g_disconnect_count++; }

static int g_pdp_deact_count;
static void on_pdp_deact(void *user) { (void)user; g_pdp_deact_count++; }

static int g_restart_count;
static void on_restart(void *user) { (void)user; g_restart_count++; }

static int g_nmea_count;
static char g_nmea_line[256];
static void on_nmea(void *user, const char *line)
{
    (void)user;
    g_nmea_count++;
    strncpy(g_nmea_line, line, sizeof(g_nmea_line) - 1);
}

static int g_csq_value;
static void on_csq(void *user, int rssi) { (void)user; g_csq_value = rssi; }

static void cb_reset(void)
{
    g_mqtt_rx_count = 0;
    g_disconnect_count = 0;
    g_pdp_deact_count = 0;
    g_restart_count = 0;
    g_nmea_count = 0;
    g_csq_value = -1;
    memset(g_mqtt_rx_topic, 0, sizeof(g_mqtt_rx_topic));
    memset(g_mqtt_rx_payload, 0, sizeof(g_mqtt_rx_payload));
    memset(g_nmea_line, 0, sizeof(g_nmea_line));
}

/* ---------- helpers ---------- */

static bool starts_with(const char *line, const char *prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    air780eg_t dev;
    mock_reset();
    ASSERT_EQ(AIR780EG_OK, air780eg_init(&dev, &g_mock_io));
    ASSERT_EQ(AIR780EG_CMD_IDLE, air780eg_cmd_status(&dev));
}

TEST(test_init_null)
{
    air780eg_t dev;
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_init(NULL, &g_mock_io));
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_init(&dev, NULL));
}

TEST(test_init_missing_send)
{
    air780eg_t dev;
    air780eg_io_t bad = g_mock_io;
    bad.send = NULL;
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_init(&dev, &bad));
}

TEST(test_send_cmd_formats_correctly)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", "OK", 1000);
    ASSERT_TRUE(g_tx_len >= 4);
    ASSERT_TRUE(memcmp(g_tx_buf, "AT\r\n", 4) == 0);
}

TEST(test_cmd_gets_ok)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", "OK", 1000);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

TEST(test_cmd_gets_error)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+UNKNOWN", "OK", 1000);
    feed_line(&dev, "ERROR");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));
}

TEST(test_cmd_gets_cme_error)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CPIN?", "OK", 1000);
    feed_line(&dev, "+CME ERROR: 10");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));
}

TEST(test_cmd_timeout)
{
    air780eg_t dev;
    mock_reset();
    g_tick_ms = 0;
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", "OK", 1000);
    g_tick_ms = 1001;
    air780eg_poll(&dev);
    ASSERT_EQ(AIR780EG_CMD_TIMEOUT, air780eg_cmd_status(&dev));
}

TEST(test_cmd_busy)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", "OK", 1000);
    ASSERT_EQ(AIR780EG_BUSY, air780eg_send_cmd(&dev, "AT+CSQ", "OK", 1000));
}

TEST(test_cmd_custom_expect)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CGSN", NULL, 1000);
    feed_line(&dev, "862345678901234");
    /* Without expect, "OK" is still matched */
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_prev_line(&dev), "862345678901234") == 0);
}

TEST(test_urc_mqtt_rx)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"/flybox/123/down/cmd\",11,\"hello world\"");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_TRUE(strcmp(g_mqtt_rx_topic, "/flybox/123/down/cmd") == 0);
    ASSERT_EQ(11, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, "hello world", 11) == 0);
}

TEST(test_urc_disconnect)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_disconnect(&dev, on_disconnect, NULL);
    feed_line(&dev, "+IPCLOSE: 0,1");
    ASSERT_EQ(1, g_disconnect_count);
}

TEST(test_urc_pdp_deact)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_pdp_deact(&dev, on_pdp_deact, NULL);
    feed_line(&dev, "+CGEV: NW DEACT 1");
    ASSERT_EQ(1, g_pdp_deact_count);
}

TEST(test_urc_rdy)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_restart(&dev, on_restart, NULL);
    feed_line(&dev, "RDY");
    ASSERT_EQ(1, g_restart_count);
    ASSERT_EQ(1u, air780eg_get_rdy_count(&dev));
    feed_line(&dev, "RDY");
    ASSERT_EQ(2, g_restart_count);
    ASSERT_EQ(2u, air780eg_get_rdy_count(&dev));
}

TEST(test_urc_nmea)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_nmea(&dev, on_nmea, NULL);
    feed_line(&dev, "$GNGGA,123456.00,3000.0000,N,12000.0000,E,1,12,0.8,100.0,M,,M,,*XX");
    ASSERT_EQ(1, g_nmea_count);
    ASSERT_TRUE(starts_with(g_nmea_line, "$GNGGA"));
}

TEST(test_urc_csq)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_csq(&dev, on_csq, NULL);
    feed_line(&dev, "+CSQ: 23,99");
    ASSERT_EQ(23, g_csq_value);
    ASSERT_EQ(23, air780eg_get_last_csq(&dev));
}

TEST(test_feed_null_safe)
{
    air780eg_feed(NULL, 'A');
    /* Should not crash */
}

TEST(test_poll_null_safe)
{
    air780eg_poll(NULL);
}

TEST(test_diagnostics_null)
{
    ASSERT_EQ(0u, air780eg_get_rdy_count(NULL));
    ASSERT_EQ(-1, air780eg_get_last_csq(NULL));
}

TEST(test_error_codes_visible)
{
    /* Ensure ERR_NACK and ERR_TIMEOUT constants are reachable */
    ASSERT_EQ(-3, (int)AIR780EG_ERR_NACK);
    ASSERT_EQ(-2, (int)AIR780EG_ERR_TIMEOUT);
}

TEST(test_matched_line_content)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CGREG?", "+CGREG:", 1000);
    feed_line(&dev, "+CGREG: 0,1");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strstr(air780eg_get_matched_line(&dev), "+CGREG: 0,1") != NULL);
}

TEST_MAIN("air780eg")
    RUN(test_init_ok);
    RUN(test_init_null);
    RUN(test_init_missing_send);
    RUN(test_send_cmd_formats_correctly);
    RUN(test_cmd_gets_ok);
    RUN(test_cmd_gets_error);
    RUN(test_cmd_gets_cme_error);
    RUN(test_cmd_timeout);
    RUN(test_cmd_busy);
    RUN(test_cmd_custom_expect);
    RUN(test_urc_mqtt_rx);
    RUN(test_urc_disconnect);
    RUN(test_urc_pdp_deact);
    RUN(test_urc_rdy);
    RUN(test_urc_nmea);
    RUN(test_urc_csq);
    RUN(test_feed_null_safe);
    RUN(test_poll_null_safe);
    RUN(test_diagnostics_null);
    RUN(test_error_codes_visible);
    RUN(test_matched_line_content);
TEST_MAIN_END()
