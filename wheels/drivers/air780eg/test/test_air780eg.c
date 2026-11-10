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

/* Feed bytes with no line end added */
static void feed_bytes(air780eg_t *dev, const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        air780eg_feed(dev, (uint8_t)data[i]);
    }
}

/* ---------- callback state ---------- */

static int g_mqtt_rx_count;
static char g_mqtt_rx_topic[300];
static char g_mqtt_rx_payload[4200];
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

/* p.13: a command line ends in <CR>, and p.33: 13 is the only terminator. An LF
 * after it could become the first payload byte of a command with a data phase. */
TEST(test_send_cmd_formats_correctly)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", "OK", 1000);
    ASSERT_EQ(3, g_tx_len);
    ASSERT_TRUE(memcmp(g_tx_buf, "AT\r", 3) == 0);
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

/* p.159: the prompt is "> ", with no line end to wait for. */
TEST(test_prompt_ends_a_wait_for_it_at_once)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CIPSEND=5", ">", 2000);
    feed_bytes(&dev, "\r\n", 2);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_bytes(&dev, ">", 1);
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_matched_line(&dev), ">") == 0);
}

/* p.159: with no connection the reply is +CME ERROR, not the prompt. */
TEST(test_error_ends_a_wait_for_the_prompt)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CIPSEND=5", ">", 2000);
    feed_line(&dev, "+CME ERROR: 50");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));
}

/* The prompt opens a line: a > elsewhere is text, here in an AT+CIPSTATUS-style
 * reply, and a plain OK is no prompt either. */
TEST(test_gt_inside_a_line_is_no_prompt)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CIPSEND=5", ">", 2000);
    feed_line(&dev, "+X: a>b");
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
}

/* With no wait for the prompt, > is an ordinary first byte of a line. */
TEST(test_gt_without_a_wait_is_text)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT", NULL, 2000);
    feed_line(&dev, "> text");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_prev_line(&dev), "> text") == 0);
}

/* p.241: AT+MPUBEX gives the prompt, takes len bytes, and answers OK. The bytes
 * are data: no line end is added, and none in them has any meaning. */
TEST(test_send_data_after_the_prompt)
{
    static const uint8_t data[] = { 'a', '\r', '\n', 0, 'b' };
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+MPUBEX=\"t\",0,0,5", ">", 2000);
    feed_bytes(&dev, "\r\n> ", 4);
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));

    g_tx_len = 0;
    ASSERT_EQ(AIR780EG_OK, air780eg_send_data(&dev, data, 5, NULL, 9000));
    ASSERT_EQ(5, g_tx_len);
    ASSERT_TRUE(memcmp(g_tx_buf, data, 5) == 0);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* The space of the prompt must not spoil a reply with no line end before it. */
TEST(test_prompt_space_does_not_spoil_the_reply)
{
    static const uint8_t data[] = { 'x' };
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+MPUBEX=\"t\",0,0,1", ">", 2000);
    feed_bytes(&dev, "> ", 2);
    ASSERT_EQ(AIR780EG_OK, air780eg_send_data(&dev, data, 1, NULL, 9000));
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_matched_line(&dev), "OK") == 0);
}

/* p.159: after the data AT+CIPSEND answers SEND OK or SEND FAIL. */
TEST(test_send_data_waits_for_its_expect)
{
    static const uint8_t data[] = { 'h', 'i' };
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    ASSERT_EQ(AIR780EG_OK, air780eg_send_data(&dev, data, 2, "SEND OK", 9000));
    feed_line(&dev, "SEND OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));

    ASSERT_EQ(AIR780EG_OK, air780eg_send_data(&dev, data, 2, "SEND OK", 9000));
    feed_line(&dev, "SEND FAIL");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));

    ASSERT_EQ(AIR780EG_OK, air780eg_send_data(&dev, data, 2, "SEND OK", 100));
    g_tick_ms += 100;
    air780eg_poll(&dev);
    ASSERT_EQ(AIR780EG_CMD_TIMEOUT, air780eg_cmd_status(&dev));
}

TEST(test_send_data_refuses_bad_calls)
{
    static const uint8_t data[] = { 'x' };
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_send_data(NULL, data, 1, NULL, 100));
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_send_data(&dev, NULL, 1, NULL, 100));
    ASSERT_EQ(AIR780EG_ERR_PARAM, air780eg_send_data(&dev, data, 0, NULL, 100));
    ASSERT_EQ(0, g_tx_len);
    ASSERT_EQ(AIR780EG_CMD_IDLE, air780eg_cmd_status(&dev));

    air780eg_send_cmd(&dev, "AT", NULL, 100);
    g_tx_len = 0;
    ASSERT_EQ(AIR780EG_BUSY, air780eg_send_data(&dev, data, 1, NULL, 100));
    ASSERT_EQ(0, g_tx_len);
}

/* p.24: ATE1 echoes the command, so the echo must not count as its reply. */
TEST(test_echo_is_not_a_reply)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+CGREG?", "CGREG", 1000);
    feed_line(&dev, "AT+CGREG?");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    ASSERT_EQ('\0', (int)air780eg_get_prev_line(&dev)[0]);
    feed_line(&dev, "+CGREG: 0,1");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_matched_line(&dev), "+CGREG: 0,1") == 0);
}

/* An echo is an echo however long the command is, and once only: the same text
 * coming back a second time is not filtered. */
TEST(test_echo_of_a_long_command_is_not_a_reply)
{
    char cmd[120];
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    strcpy(cmd, "AT+MPUB=\"t\",0,0,\"");
    memset(cmd + strlen(cmd), 'x', 90);
    cmd[17 + 90] = '\0';
    strcat(cmd, "\"");
    air780eg_send_cmd(&dev, cmd, "xxx", 1000);
    feed_line(&dev, cmd);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, cmd);
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* Found by the fuzzer: an echo is the whole line, not the line's first bytes. */
TEST(test_reply_that_starts_like_the_command_is_no_echo)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+C", NULL, 1000);
    feed_line(&dev, "AT+CSQ");
    ASSERT_TRUE(strcmp(air780eg_get_prev_line(&dev), "AT+CSQ") == 0);
}

/* p.13: command lines start with AT, so other text sent this way has no echo to
 * filter, and the first reply must not be taken for one. */
TEST(test_text_that_is_no_command_has_no_echo)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "+", NULL, 1000);
    feed_line(&dev, "+CME ERROR: 3");
    ASSERT_EQ(AIR780EG_CMD_ERROR, air780eg_cmd_status(&dev));

    air780eg_send_cmd(&dev, "OK", NULL, 1000);
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* p.13 allows lower case as well. */
TEST(test_echo_of_a_lower_case_command_is_not_a_reply)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "at+cgreg?", "cgreg", 1000);
    feed_line(&dev, "at+cgreg?");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
}

/* With echo off nothing changes, and a command that ends in FAIL is no failure
 * when it is only its own echo. */
TEST(test_echo_of_a_fail_token_is_not_a_failure)
{
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+X=FAIL", NULL, 1000);
    feed_line(&dev, "AT+X=FAIL");
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* Data sent after the prompt has no echo to wait for. */
TEST(test_send_data_clears_the_echo_of_the_command)
{
    static const uint8_t data[] = { 'x' };
    air780eg_t dev;
    mock_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_send_cmd(&dev, "AT+TOKEN", ">", 1000);
    feed_bytes(&dev, "> ", 2);
    air780eg_send_data(&dev, data, 1, "AT+TOKEN", 1000);
    feed_line(&dev, "AT+TOKEN");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* A reply can arrive while send is still running: from then on the line belongs
 * to feed, so nothing set up for the command may come after the first byte. */
static air780eg_t *g_inline_dev;
static void send_and_answer(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)data; (void)len;
    feed_line(g_inline_dev, "OK");
}

TEST(test_reply_during_send_is_not_lost)
{
    air780eg_t dev;
    air780eg_io_t io = g_mock_io;
    io.send = send_and_answer;
    mock_reset();
    g_inline_dev = &dev;
    air780eg_init(&dev, &io);
    air780eg_send_cmd(&dev, "AT", NULL, 1000);
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_TRUE(strcmp(air780eg_get_matched_line(&dev), "OK") == 0);
}

TEST(test_urc_mqtt_rx)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"/dev/123/down/cmd\",11 byte,hello world");
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

    /* Feed an +MSUB URC while command is pending: mqtt_rx fires and
     * cmd_status stays PENDING. */
    feed_line(&dev, "+MSUB: \"/t\",3 byte,abc");
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
    /* The syntax line of p.243 has a bare <len>, the samples add the word byte */
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
    /* +MSUB without a comma after the topic: no <len>, so no message. */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"topic\"");
    ASSERT_EQ(0, g_mqtt_rx_count);
}

/* <topic> is at most 256 bytes (p.243), and such a topic must arrive whole. A
 * longer one is not legal, and a shortened topic could match another
 * subscription, so it is dropped and counted. */
TEST(test_msub_long_topic_is_dropped)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);

    char line[300];
    strcpy(line, "+MSUB: \"");
    size_t base = strlen(line);
    memset(line + base, 'A', 257);
    strcpy(line + base + 257, "\",3 byte,abc");
    feed_line(&dev, line);
    ASSERT_EQ(0, g_mqtt_rx_count);
    ASSERT_EQ(1u, air780eg_get_msg_drop_count(&dev));

    strcpy(line + base + 256, "\",3 byte,abc");
    feed_line(&dev, line);
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(256, (int)strlen(g_mqtt_rx_topic));
    ASSERT_EQ(3, g_mqtt_rx_payload_len);
    ASSERT_EQ(1u, air780eg_get_msg_drop_count(&dev));
}

/* What AT+MQTTMSGGET prints, p.247: the topic has no quotes. */
TEST(test_msub_unquoted_topic_from_the_cache)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);

    air780eg_send_cmd(&dev, "AT+MQTTMSGGET", NULL, 1000);
    feed_line(&dev, "+MSUB: mqtt/topic,9 byte,SSSSddddd");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_TRUE(strcmp(g_mqtt_rx_topic, "mqtt/topic") == 0);
    ASSERT_EQ(9, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, "SSSSddddd", 9) == 0);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* A quoted topic may hold a comma: only the quote and the comma end it. */
TEST(test_msub_comma_in_a_quoted_topic)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"a,b\",2 byte,hi");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_TRUE(strcmp(g_mqtt_rx_topic, "a,b") == 0);
    ASSERT_EQ(2, g_mqtt_rx_payload_len);
}

/* <message> is at most 4100 bytes (p.243). */
TEST(test_msub_longest_message_arrives_whole)
{
    static char payload[4100];
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (char)('a' + i % 26);
    }
    const char *head = "+MSUB: \"t\",4100 byte,";
    feed_bytes(&dev, head, strlen(head));
    feed_bytes(&dev, payload, sizeof(payload));
    feed_bytes(&dev, "\r\n", 2);
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(4100, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, payload, sizeof(payload)) == 0);
}

/* The bytes of a message are data, whatever they look like: they neither split
 * the message nor reach the command or URC matching. */
TEST(test_msub_payload_with_line_ends_stays_whole)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    air780eg_on_restart(&dev, on_restart, NULL);

    air780eg_send_cmd(&dev, "AT", NULL, 1000);
    feed_line(&dev, "+MSUB: \"t\",14 byte,a\r\nOK\r\nRDY\r\nb,");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(14, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, "a\r\nOK\r\nRDY\r\nb,", 14) == 0);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    ASSERT_EQ(0, g_restart_count);
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* The payload has no quotes around it (p.248), so a quote in it is data. */
TEST(test_msub_payload_quotes_are_data)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"t\",7 byte,\"a\":\"b\"");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(7, g_mqtt_rx_payload_len);
    ASSERT_TRUE(memcmp(g_mqtt_rx_payload, "\"a\":\"b\"", 7) == 0);
}

TEST(test_msub_empty_message)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"t\",0 byte,");
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(0, g_mqtt_rx_payload_len);
    ASSERT_TRUE(strcmp(g_mqtt_rx_topic, "t") == 0);
}

/* More than the 4100 of p.243 is a garbled line: dropped and counted, and the
 * parser stays on lines instead of swallowing what follows. */
TEST(test_msub_impossible_length_stays_on_lines)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    air780eg_send_cmd(&dev, "AT", NULL, 1000);
    feed_line(&dev, "+MSUB: \"t\",4101 byte,a,b,c");
    feed_line(&dev, "+MSUB: \"t\",99999999999999999999 byte,abc");
    ASSERT_EQ(0, g_mqtt_rx_count);
    ASSERT_EQ(2u, air780eg_get_msg_drop_count(&dev));
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
}

/* Bytes lost on the UART must not leave the parser waiting for a length that
 * never comes: it gives the message up, never delivers it cut, and hears the
 * next reply. */
TEST(test_msub_stalled_message_is_given_up)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);

    const char *head = "+MSUB: \"t\",100 byte,abc";
    feed_bytes(&dev, head, strlen(head));
    air780eg_poll(&dev);
    g_tick_ms = AIR780EG_MSG_STALL_MS - 1;
    air780eg_poll(&dev);
    feed_bytes(&dev, "d", 1);          /* still moving */
    air780eg_poll(&dev);
    g_tick_ms += AIR780EG_MSG_STALL_MS;
    air780eg_poll(&dev);

    air780eg_send_cmd(&dev, "AT", NULL, 100000);
    feed_line(&dev, "OK");
    ASSERT_EQ(AIR780EG_CMD_OK, air780eg_cmd_status(&dev));
    ASSERT_EQ(0, g_mqtt_rx_count);
    ASSERT_EQ(1u, air780eg_get_msg_drop_count(&dev));
}

/* Traffic that keeps moving is no stall, however long it takes. */
TEST(test_msub_slow_message_is_not_a_stall)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);

    const char *head = "+MSUB: \"t\",4 byte,";
    feed_bytes(&dev, head, strlen(head));
    for (int i = 0; i < 4; i++) {
        air780eg_poll(&dev);
        g_tick_ms += AIR780EG_MSG_STALL_MS - 1;
        air780eg_poll(&dev);
        feed_bytes(&dev, "x", 1);
    }
    ASSERT_EQ(1, g_mqtt_rx_count);
    ASSERT_EQ(4, g_mqtt_rx_payload_len);
    ASSERT_EQ(0u, air780eg_get_msg_drop_count(&dev));
}

/* A message reads the same with no callback, so its bytes never become lines. */
TEST(test_msub_without_a_callback_skips_the_payload)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_restart(&dev, on_restart, NULL);
    feed_line(&dev, "+MSUB: \"t\",5 byte,\r\nRDY");
    feed_line(&dev, "RDY");
    ASSERT_EQ(1, g_restart_count);
    ASSERT_EQ(0u, air780eg_get_msg_drop_count(&dev));
}

static int g_cached_count;
static int g_cached_addr;
static void on_cached(void *user, uint8_t store_addr)
{
    (void)user;
    g_cached_count++;
    g_cached_addr = store_addr;
}

/* Cache mode, p.243 and p.247: +MSUB: <store_addr>, 0 to 3, says a message is
 * waiting for AT+MQTTMSGGET. */
TEST(test_msub_store_addr_reaches_the_callback)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    g_cached_count = 0;
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    air780eg_on_mqtt_cached(&dev, on_cached, NULL);

    feed_line(&dev, "+MSUB: 0");
    ASSERT_EQ(1, g_cached_count);
    ASSERT_EQ(0, g_cached_addr);
    feed_line(&dev, "+MSUB: 3");
    ASSERT_EQ(2, g_cached_count);
    ASSERT_EQ(3, g_cached_addr);

    /* also while a command is pending */
    air780eg_send_cmd(&dev, "AT+CSQ", "+CSQ:", 1000);
    feed_line(&dev, "+MSUB: 1");
    ASSERT_EQ(3, g_cached_count);
    ASSERT_EQ(1, g_cached_addr);
    ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
    ASSERT_EQ(0, g_mqtt_rx_count);
}

TEST(test_msub_other_short_lines_are_no_store_addr)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    g_cached_count = 0;
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_cached(&dev, on_cached, NULL);

    feed_line(&dev, "+MSUB: 4");
    feed_line(&dev, "+MSUB: 01");
    feed_line(&dev, "+MSUB:");
    feed_line(&dev, "+MSUB: x");
    ASSERT_EQ(0, g_cached_count);

    /* no callback registered must be safe */
    air780eg_on_mqtt_cached(&dev, NULL, NULL);
    feed_line(&dev, "+MSUB: 0");
    ASSERT_EQ(0, g_cached_count);
}

/* The reply to AT+MSUB=? (p.243) is no message. */
TEST(test_msub_test_reply_is_not_a_message)
{
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB:<topic>,(0-2)");
    ASSERT_EQ(0, g_mqtt_rx_count);
    ASSERT_EQ(0u, air780eg_get_msg_drop_count(&dev));
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
    feed_line(&dev, "+MSUB: \"/t\",3 byte,abc");
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
    /* An unquoted topic with no <len> after it is no message */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: no_quote_here");
    ASSERT_EQ(0, g_mqtt_rx_count);
}

TEST(test_msub_no_closing_topic_quote)
{
    /* +MSUB: "unclosed_topic: the topic never ends, so there is no message */
    air780eg_t dev;
    mock_reset();
    cb_reset();
    air780eg_init(&dev, &g_mock_io);
    air780eg_on_mqtt_rx(&dev, on_mqtt_rx, NULL);
    feed_line(&dev, "+MSUB: \"unclosed_topic");
    ASSERT_EQ(0, g_mqtt_rx_count);
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
    RUN(test_prompt_ends_a_wait_for_it_at_once);
    RUN(test_error_ends_a_wait_for_the_prompt);
    RUN(test_gt_inside_a_line_is_no_prompt);
    RUN(test_gt_without_a_wait_is_text);
    RUN(test_send_data_after_the_prompt);
    RUN(test_prompt_space_does_not_spoil_the_reply);
    RUN(test_send_data_waits_for_its_expect);
    RUN(test_send_data_refuses_bad_calls);
    RUN(test_echo_is_not_a_reply);
    RUN(test_echo_of_a_long_command_is_not_a_reply);
    RUN(test_reply_that_starts_like_the_command_is_no_echo);
    RUN(test_text_that_is_no_command_has_no_echo);
    RUN(test_echo_of_a_lower_case_command_is_not_a_reply);
    RUN(test_echo_of_a_fail_token_is_not_a_failure);
    RUN(test_send_data_clears_the_echo_of_the_command);
    RUN(test_reply_during_send_is_not_lost);
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
    RUN(test_msub_unquoted_topic_from_the_cache);
    RUN(test_msub_comma_in_a_quoted_topic);
    RUN(test_msub_longest_message_arrives_whole);
    RUN(test_msub_payload_with_line_ends_stays_whole);
    RUN(test_msub_payload_quotes_are_data);
    RUN(test_msub_empty_message);
    RUN(test_msub_impossible_length_stays_on_lines);
    RUN(test_msub_stalled_message_is_given_up);
    RUN(test_msub_slow_message_is_not_a_stall);
    RUN(test_msub_without_a_callback_skips_the_payload);
    RUN(test_msub_store_addr_reaches_the_callback);
    RUN(test_msub_other_short_lines_are_no_store_addr);
    RUN(test_msub_test_reply_is_not_a_message);
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
