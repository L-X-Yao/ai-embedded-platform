// SPDX-License-Identifier: MIT
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

/* p.240: the OK only says the command was taken, CONNACK OK comes later. */
TEST(test_plain_ok_does_not_end_a_wait_for_connack)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+MCONNECT=1,60", "CONNACK OK", 9000);
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "CONNACK OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_matched_line(&dev), "CONNACK OK") == 0);
}

/* p.240: a failed session reports ERROR after the OK, and it must not be lost. */
TEST(test_error_after_the_ok_ends_the_wait)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+MCONNECT=1,60", "CONNACK OK", 9000);
    feed_line(&dev, "OK");
    feed_line(&dev, "ERROR");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));
}

/* p.243 and p.247: OK then SUBACK. */
TEST(test_plain_ok_does_not_end_a_wait_for_suback)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+MSUB=\"mqtt/topic\",0", "SUBACK", 9000);
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "SUBACK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* With expect OK or none, the final result is a line that is exactly OK: not
 * the tail of CONNECT OK, which arrives after the command has ended anyway. */
TEST(test_expect_ok_needs_the_whole_line)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CIPSTATUS", "OK", 1000);
    feed_line(&dev, "STATE: CONNECT OK");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_prev_line(&dev), "STATE: CONNECT OK") == 0);
}

/* p.239: OK, then STATE:<state> and CONNECT FAIL. */
TEST(test_connect_fail_ends_the_command)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+MIPSTART=\"a.b\",1883", "CONNECT OK", 9000);
    feed_line(&dev, "OK");
    feed_line(&dev, "STATE: IP INITIAL");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "CONNECT FAIL");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_matched_line(&dev), "CONNECT FAIL") == 0);
    ASSERT_TRUE(strcmp(air780eg_get_prev_line(&dev), "STATE: IP INITIAL") == 0);
}

/* p.155 and p.159: the multi-link forms carry the link number first. */
TEST(test_fail_forms_of_the_manual_end_the_command)
{
    static const char *const fails[] = {
        "7,CONNECT FAIL", "SEND FAIL", "2,SEND FAIL",
    };
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    for (size_t i = 0; i < sizeof(fails) / sizeof(fails[0]); i++) {
        ASSERT_EQ(AIR780EG_OK, air780eg_send_cmd(&dev, "AT+CIPSEND=5", "SEND OK", 9000));
        feed_line(&dev, fails[i]);
        ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));
    }
}

/* p.155: CONNECT is the transparent-mode success, and it is inside CONNECT FAIL. */
TEST(test_fail_line_wins_over_an_expect_inside_it)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CIPSTART=\"TCP\",\"a.b\",80", "CONNECT", 9000);
    feed_line(&dev, "CONNECT FAIL");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));
}

static int g_fail_count;
static char g_fail_line[64];
static void on_fail(void *user, const char *line)
{
    (void)user;
    g_fail_count++;
    strncpy(g_fail_line, line, sizeof(g_fail_line) - 1);
}

/* The URC can come after the command that caused it has ended. */
TEST(test_fail_with_nothing_pending_reaches_the_callback)
{
    air780eg_t dev;
    mock_reset();
    g_fail_count = 0;
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_fail(&dev, on_fail, NULL);

    air780eg_send_cmd(&dev, "AT+MIPSTART=\"a.b\",1883", NULL, 9000);
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    feed_line(&dev, "CONNECT FAIL");
    ASSERT_EQ(1, g_fail_count);
    ASSERT_TRUE(strcmp(g_fail_line, "CONNECT FAIL") == 0);
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));

    /* while a command is pending the line ends it instead */
    air780eg_send_cmd(&dev, "AT", NULL, 1000);
    feed_line(&dev, "SEND FAIL");
    ASSERT_EQ(1, g_fail_count);
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));

    /* no callback registered must be safe */
    air780eg_on_fail(&dev, NULL, NULL);
    feed_line(&dev, "SEND FAIL");
    ASSERT_EQ(1, g_fail_count);
}

TEST(test_urc_mqtt_rx)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"/dev/123/down/cmd\",11,\"hello world\"");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_TRUE(strcmp(g_mqtt_rx_topic, "/dev/123/down/cmd") == 0);
    ASSERT_EQ(11, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, "hello world", 11) == 0);
}

/* The manual's direct-mode sample, p.248: <- (URC) +MSUB: "mqtt/topic",9 byte,
 * SSSSddddd. The payload has no quotes and the length carries the word byte. */
TEST(test_urc_mqtt_rx_manual_sample)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"mqtt/topic\",9 byte,SSSSddddd");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_TRUE(strcmp(g_mqtt_rx_topic, "mqtt/topic") == 0);
    ASSERT_EQ(9, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, "SSSSddddd", 9) == 0);
}

TEST(test_urc_disconnect)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_disconnect(&dev, on_disconnect, NULL);
    feed_line(&dev, "CLOSED");
    ASSERT_EQ(1, g_disconnect_count);
}

/* +IPCLOSE: is in neither the manual nor flybox's board-tested driver. The
 * documented TCP drop is CLOSED (p.249). */
TEST(test_urc_ipclose_is_not_a_documented_drop)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_disconnect(&dev, on_disconnect, NULL);
    feed_line(&dev, "+IPCLOSE: 0,1");
    ASSERT_EQ(0, g_disconnect_count);
}

/* The manual's strings: +CGEV: NW PDN DEACT 1 (p.138), +CGEV: ME PDN DEACT
 * (p.137), +PDP DEACT (p.249) and +PDP:DEACT (p.173). */
TEST(test_urc_pdp_deact)
{
    static const char *const urcs[] = {
        "+CGEV: NW PDN DEACT 1",
        "+CGEV: ME PDN DEACT 1",
        "+CGEV: ME PDN DEACT 1,2",
        "+PDP DEACT",
        "+PDP:DEACT",
    };
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_pdp_deact(&dev, on_pdp_deact, NULL);
    for (size_t i = 0; i < sizeof(urcs) / sizeof(urcs[0]); i++) {
        feed_line(&dev, urcs[i]);
        ASSERT_EQ((int)i + 1, g_pdp_deact_count);
    }
}

/* AT+CIPSTATUS prints the state name PDP DEACT (p.163): a status reply, not the
 * URC, and the activation event of p.138 is not a loss either. */
TEST(test_status_lines_are_not_a_pdp_deact)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_pdp_deact(&dev, on_pdp_deact, NULL);
    feed_line(&dev, "STATE: PDP DEACT");
    feed_line(&dev, "+CGEV: ME PDN ACT 1,0");
    ASSERT_EQ(0, g_pdp_deact_count);
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

TEST(test_send_cmd_null_cmd)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_send_cmd(&dev, NULL, "OK", 1000));
}

TEST(test_cmd_reissue_after_ok)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", "OK", 1000);
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    /* cmd_status is now CMD_OK (not PENDING), so a new send is allowed */
    ASSERT_EQ(AIR780EG_OK, air780eg_send_cmd(&dev, "AT+CSQ", "+CSQ:", 1000));
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
}

TEST(test_urc_during_pending_cmd)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);

    /* Start a command that expects +CSQ: */
    air780eg_send_cmd(&dev, "AT+CSQ", "+CSQ:", 5000);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));

    /* Feed an +MSUB URC while command is pending.
     * process_line stores line in prev_line then calls dispatch_urc,
     * so mqtt_rx fires but cmd_status stays PENDING. */
    feed_line(&dev, "+MSUB: \"/t\",3,\"abc\"");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));

    /* Now complete the command */
    feed_line(&dev, "+CSQ: 15,99");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

TEST(test_timeout_tick_overflow)
{
    /* Verify timeout works correctly across uint32 wrap-around.
     * start=0xFFFFFFF0, timeout=100. At tick 0xFFFFFFF5 (elapsed=5) → still pending.
     * At tick 0x00000054 (elapsed=100) → timeout fires. */
    air780eg_t dev;
    mock_reset();
    g_tick_ms = 0xFFFFFFF0u;
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", "OK", 100);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));

    /* 5 ms elapsed — should NOT timeout */
    g_tick_ms = 0xFFFFFFF5u;
    air780eg_poll(&dev);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));

    /* 99 ms elapsed — still pending */
    g_tick_ms = 0xFFFFFFF0u + 99u;
    air780eg_poll(&dev);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));

    /* 100 ms elapsed (wraps to 0x00000054) — now timeout */
    g_tick_ms = 0xFFFFFFF0u + 100u;
    air780eg_poll(&dev);
    ASSERT_EQ(AIR780EG_CMD_TIMEOUT, air780eg_cmd_status(&dev));
}

TEST(test_msub_unquoted_payload)
{
    /* Payload without surrounding quotes: +MSUB: "topic",5,hello */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"topic\",5,hello");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(5, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, "hello", 5) == 0);
}

TEST(test_msub_malformed_no_comma)
{
    /* +MSUB without a comma after the topic — second strchr returns NULL,
     * dispatch_urc returns early, callback must NOT fire. */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"topic\"");
    ASSERT_EQ(0, g_mqtt_rx_count);
}

TEST(test_msub_long_topic_is_dropped)
{
    /* Topic of 200 'A' chars — exceeds AIR780EG_TOPIC_MAX (128).
     * Implementation clamps tlen to sizeof(topic)-1 = 127, NUL-terminates,
     * and still fires the callback. */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);

    /* Build the line: +MSUB: "<200 As>",3,abc */
    char line[300];
    strcpy(line, "+MSUB: \"");
    int base = (int)strlen(line);
    int i;
    for (i = 0; i < 200; i++) {
        line[base + i] = 'A';
    }
    line[base + i] = '\0';
    strcat(line, "\",3,abc");
    feed_line(&dev, line);

    /* a shortened topic could match a different subscription */
    ASSERT_EQ(0, g_mqtt_rx_count);

    /* the longest topic that fits arrives whole */
    line[base + 127] = '\0';
    strcat(line, "\",3,abc");
    feed_line(&dev, line);
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(127, (int)strlen(g_mqtt_rx_topic));
}

TEST(test_urc_closed_bare)
{
    /* Bare "CLOSED" line, the manual's TCP drop URC (p.249), triggers the
     * disconnect callback via starts_with check */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_disconnect(&dev, on_disconnect, NULL);
    feed_line(&dev, "CLOSED");
    ASSERT_EQ(1, g_disconnect_count);
}

TEST(test_urc_pdp_deact_bare)
{
    /* The form the manual tells the host to act on, p.249 */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_pdp_deact(&dev, on_pdp_deact, NULL);
    feed_line(&dev, "+PDP DEACT");
    ASSERT_EQ(1, g_pdp_deact_count);
}

TEST(test_feed_overflow_protection)
{
    /* Feed 600 bytes without a newline — buffer holds AIR780EG_LINE_MAX-1=511
     * chars then stops accumulating. Feeding \r\n afterward processes the
     * truncated line without crash. */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);

    int j;
    for (j = 0; j < 600; j++) {
        air780eg_feed(&dev, (uint8_t)'X');
    }
    /* Terminate the line — must not crash */
    air780eg_feed(&dev, '\r');
    air780eg_feed(&dev, '\n');
    /* No specific output assertion; the goal is no crash and continued operation */
    ASSERT_EQ(AIR780EG_CMD_IDLE, air780eg_cmd_status(&dev));
}

TEST(test_csq_zero)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_csq(&dev, on_csq, NULL);
    feed_line(&dev, "+CSQ: 0,99");
    ASSERT_EQ(0, air780eg_get_last_csq(&dev));
    ASSERT_EQ(0, g_csq_value);
}

TEST(test_csq_99)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_csq(&dev, on_csq, NULL);
    feed_line(&dev, "+CSQ: 99,99");
    ASSERT_EQ(99, air780eg_get_last_csq(&dev));
    ASSERT_EQ(99, g_csq_value);
}

TEST(test_cms_error)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CMGS", "OK", 1000);
    feed_line(&dev, "+CMS ERROR: 500");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));
}

TEST(test_deregister_callback_null)
{
    /* Register then clear the mqtt_rx callback by passing NULL.
     * Feeding +MSUB afterwards must not invoke any callback. */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    /* Deregister */
    air780eg_on_mqtt_rx(&dev, NULL, NULL);
    feed_line(&dev, "+MSUB: \"/t\",3,\"abc\"");
    ASSERT_EQ(0, g_mqtt_rx_count);
}

TEST(test_urc_nmea_gnrmc)
{
    /* $GNRMC NMEA sentence triggers nmea callback */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_nmea(&dev, on_nmea, NULL);
    feed_line(&dev, "$GNRMC,123456.00,A,3000.0000,N,12000.0000,E,0.00,0.00,010101,,,A*XX");
    ASSERT_EQ(1, g_nmea_count);
    ASSERT_TRUE(starts_with(g_nmea_line, "$GNRMC"));
}

TEST(test_urc_nmea_gngsa)
{
    /* $GNGSA NMEA sentence triggers nmea callback */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_nmea(&dev, on_nmea, NULL);
    feed_line(&dev, "$GNGSA,A,3,01,02,03,,,,,,,,,1.5,0.9,1.2*XX");
    ASSERT_EQ(1, g_nmea_count);
    ASSERT_TRUE(starts_with(g_nmea_line, "$GNGSA"));
}

TEST(test_urc_nmea_gpgga)
{
    /* $GPGGA NMEA sentence triggers nmea callback */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_nmea(&dev, on_nmea, NULL);
    feed_line(&dev, "$GPGGA,123456.00,3000.0000,N,12000.0000,E,1,08,0.9,10.0,M,,M,,*XX");
    ASSERT_EQ(1, g_nmea_count);
    ASSERT_TRUE(starts_with(g_nmea_line, "$GPGGA"));
}

TEST(test_urc_nmea_gprmc)
{
    /* $GPRMC NMEA sentence triggers nmea callback */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_nmea(&dev, on_nmea, NULL);
    feed_line(&dev, "$GPRMC,123456.00,A,3000.0000,N,12000.0000,E,0.00,0.00,010101,,,A*XX");
    ASSERT_EQ(1, g_nmea_count);
    ASSERT_TRUE(starts_with(g_nmea_line, "$GPRMC"));
}

TEST(test_urc_nmea_no_cb)
{
    /* NMEA URC without registered callback must not crash */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    /* No callback registered */
    feed_line(&dev, "$GNGGA,123456.00,3000.0000,N,12000.0000,E,1,12,0.8,100.0,M,,M,,*XX");
    ASSERT_EQ(0, g_nmea_count);
}

TEST(test_msub_no_opening_quote)
{
    /* +MSUB: where payload area starts without '"' — should return early */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    /* No '"' after the space — dispatch_urc returns early, cb not fired */
    feed_line(&dev, "+MSUB: no_quote_here");
    ASSERT_EQ(0, g_mqtt_rx_count);
}

TEST(test_msub_no_closing_topic_quote)
{
    /* +MSUB: "unclosed_topic — strchr for closing '"' returns NULL */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"unclosed_topic");
    ASSERT_EQ(0, g_mqtt_rx_count);
}

TEST(test_msub_quoted_payload_no_closing_quote)
{
    /* Quoted payload with no closing '"': +MSUB: "t",3,"payload_no_end
     * strrchr returns NULL so end = p + strlen(p), still fires callback */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"t\",7,\"noquote");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_TRUE(strcmp(g_mqtt_rx_topic, "t") == 0);
    /* payload is "noquote" — 7 bytes */
    ASSERT_EQ(7, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, "noquote", 7) == 0);
}

TEST(test_init_missing_get_tick)
{
    /* io with NULL get_tick_ms must be rejected */
    air780eg_t dev;
    air780eg_io_t bad = g_mock_io;
    bad.get_tick_ms = NULL;
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_init(&dev, &bad));
}

TEST(test_poll_no_op_when_idle)
{
    /* poll when not pending must not change status */
    air780eg_t dev;
    mock_reset();
    g_tick_ms = 0;
    air780eg_init(&dev, &g_mock_io);
    ASSERT_EQ(AIR780EG_CMD_IDLE, air780eg_cmd_status(&dev));
    g_tick_ms = 99999;
    air780eg_poll(&dev);
    ASSERT_EQ(AIR780EG_CMD_IDLE, air780eg_cmd_status(&dev));
}

TEST(test_send_cmd_null_expect_clears_expect)
{
    /* send_cmd with expect=NULL must clear the expect field and still OK */
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    ASSERT_EQ(AIR780EG_OK, air780eg_send_cmd(&dev, "AT", NULL, 1000));
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    /* With empty expect, only "OK" terminates the command */
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

TEST(test_urc_disconnect_no_cb)
{
    /* CLOSED URC with no disconnect callback registered must not crash */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    /* No disconnect_cb registered */
    feed_line(&dev, "CLOSED");
    ASSERT_EQ(0, g_disconnect_count);
}

TEST(test_urc_pdp_deact_no_cb)
{
    /* +PDP DEACT URC with no pdp_deact callback registered must not crash */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    feed_line(&dev, "+PDP DEACT");
    ASSERT_EQ(0, g_pdp_deact_count);
}

TEST(test_urc_rdy_no_cb)
{
    /* RDY URC without restart callback: counter increments, no crash */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    /* No restart callback */
    feed_line(&dev, "RDY");
    ASSERT_EQ(0, g_restart_count);
    ASSERT_EQ(1u, air780eg_get_rdy_count(&dev));
}

TEST(test_urc_csq_no_cb)
{
    /* +CSQ URC without csq callback: last_csq updated, no crash */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    /* No csq callback */
    feed_line(&dev, "+CSQ: 15,99");
    ASSERT_EQ(15, air780eg_get_last_csq(&dev));
}

TEST(test_matched_line_null_dev)
{
    /* get_matched_line and get_prev_line with NULL dev return empty string */
    const char *ml = air780eg_get_matched_line(NULL);
    const char *pl = air780eg_get_prev_line(NULL);
    ASSERT_TRUE(ml != NULL);
    ASSERT_TRUE(pl != NULL);
    ASSERT_EQ('\0', (int)ml[0]);
    ASSERT_EQ('\0', (int)pl[0]);
}

TEST(test_send_cmd_null_dev)
{
    /* send_cmd with NULL dev returns ERR_PARAM */
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_send_cmd(NULL, "AT", "OK", 1000));
}

TEST(test_feed_cr_lf_sequence)
{
    /* Feed data as raw bytes with \r then \n — cr is stored then stripped.
     * The standard UART sequence where \r arrives as data char then \n fires
     * the line. Use the feed helper to drive the normal path. */
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", "OK", 1000);
    /* feed_line uses \r\n — verify this produces CMD_OK (covers normal path) */
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

TEST(test_csq_outside_the_at_range_is_ignored)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_csq(&dev, on_csq, NULL);

    feed_line(&dev, "+CSQ: 100,99");
    ASSERT_EQ(-1, g_csq_value);
    /* more digits than an int holds */
    feed_line(&dev, "+CSQ: 99999999999999,99");
    ASSERT_EQ(-1, g_csq_value);
    ASSERT_EQ(-1, air780eg_get_last_csq(&dev));

    feed_line(&dev, "+CSQ: 99,99");
    ASSERT_EQ(99, g_csq_value);
}

/* p.67 defines 0 to 31 and 99, and nothing between. */
TEST(test_csq_between_31_and_99_is_ignored)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_csq(&dev, on_csq, NULL);

    feed_line(&dev, "+CSQ: 31,99");
    ASSERT_EQ(31, g_csq_value);
    g_csq_value = -1;
    feed_line(&dev, "+CSQ: 32,99");
    feed_line(&dev, "+CSQ: 98,99");
    ASSERT_EQ(-1, g_csq_value);
    ASSERT_EQ(31, air780eg_get_last_csq(&dev));
}

/* p.67: AT+CSQ=? answers with lists, so there is no reading in the line. */
TEST(test_csq_test_reply_is_not_a_reading)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_csq(&dev, on_csq, NULL);

    feed_line(&dev, "+CSQ: (0-31,99),(0-7,99)");
    feed_line(&dev, "+CSQ:");
    ASSERT_EQ(-1, g_csq_value);
    ASSERT_EQ(-1, air780eg_get_last_csq(&dev));
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
    RUN(test_plain_ok_does_not_end_a_wait_for_connack);
    RUN(test_error_after_the_ok_ends_the_wait);
    RUN(test_plain_ok_does_not_end_a_wait_for_suback);
    RUN(test_expect_ok_needs_the_whole_line);
    RUN(test_connect_fail_ends_the_command);
    RUN(test_fail_forms_of_the_manual_end_the_command);
    RUN(test_fail_line_wins_over_an_expect_inside_it);
    RUN(test_fail_with_nothing_pending_reaches_the_callback);
    RUN(test_urc_mqtt_rx);
    RUN(test_urc_mqtt_rx_manual_sample);
    RUN(test_urc_disconnect);
    RUN(test_urc_ipclose_is_not_a_documented_drop);
    RUN(test_urc_pdp_deact);
    RUN(test_status_lines_are_not_a_pdp_deact);
    RUN(test_urc_rdy);
    RUN(test_urc_nmea);
    RUN(test_urc_csq);
    RUN(test_feed_null_safe);
    RUN(test_poll_null_safe);
    RUN(test_diagnostics_null);
    RUN(test_error_codes_visible);
    RUN(test_matched_line_content);
    RUN(test_send_cmd_null_cmd);
    RUN(test_cmd_reissue_after_ok);
    RUN(test_urc_during_pending_cmd);
    RUN(test_timeout_tick_overflow);
    RUN(test_msub_unquoted_payload);
    RUN(test_msub_malformed_no_comma);
    RUN(test_msub_long_topic_is_dropped);
    RUN(test_urc_closed_bare);
    RUN(test_urc_pdp_deact_bare);
    RUN(test_feed_overflow_protection);
    RUN(test_csq_zero);
    RUN(test_csq_99);
    RUN(test_cms_error);
    RUN(test_deregister_callback_null);
    RUN(test_urc_nmea_gnrmc);
    RUN(test_urc_nmea_gngsa);
    RUN(test_urc_nmea_gpgga);
    RUN(test_urc_nmea_gprmc);
    RUN(test_urc_nmea_no_cb);
    RUN(test_msub_no_opening_quote);
    RUN(test_msub_no_closing_topic_quote);
    RUN(test_msub_quoted_payload_no_closing_quote);
    RUN(test_init_missing_get_tick);
    RUN(test_poll_no_op_when_idle);
    RUN(test_send_cmd_null_expect_clears_expect);
    RUN(test_urc_disconnect_no_cb);
    RUN(test_urc_pdp_deact_no_cb);
    RUN(test_urc_rdy_no_cb);
    RUN(test_urc_csq_no_cb);
    RUN(test_matched_line_null_dev);
    RUN(test_send_cmd_null_dev);
    RUN(test_feed_cr_lf_sequence);
    RUN(test_csq_outside_the_at_range_is_ignored);
    RUN(test_csq_between_31_and_99_is_ignored);
    RUN(test_csq_test_reply_is_not_a_reading);
TEST_MAIN_END()
