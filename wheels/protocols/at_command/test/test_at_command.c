#include "test_harness.h"
#include <string.h>
#include "at_command.h"

/* ---------- mock HAL ---------- */

#define TX_BUF_SIZE 1024

static uint8_t tx_buf[TX_BUF_SIZE];
static int     tx_len;

static void mock_reset(void)
{
    memset(tx_buf, 0, sizeof(tx_buf));
    tx_len = 0;
}

static void mock_send(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    for (uint16_t i = 0; i < len && tx_len < TX_BUF_SIZE; i++) {
        tx_buf[tx_len++] = data[i];
    }
}

static const at_command_hal_t mock_hal = {
    .send = mock_send,
    .ctx  = NULL,
};

/* ---------- URC capture ---------- */

static char urc_captured[256];
static int  urc_count;

static void urc_handler(const char *line, void *ctx)
{
    (void)ctx;
    strncpy(urc_captured, line, sizeof(urc_captured) - 1);
    urc_captured[sizeof(urc_captured) - 1] = '\0';
    urc_count++;
}

static void urc_reset(void)
{
    urc_captured[0] = '\0';
    urc_count = 0;
}

/* ---------- helpers ---------- */
static at_command_evt_type_t feed_string(at_command_t *at, const char *s, at_command_evt_t *evt)
{
    at_command_evt_type_t last = AT_COMMAND_EVT_NONE;
    while (*s) {
        at_command_evt_type_t r = at_command_feed(at, (uint8_t)*s++, evt);
        if (r != AT_COMMAND_EVT_NONE) {
            last = r;
        }
    }
    return last;
}

/* ---------- tests ---------- */

TEST(test_init)
{
    at_command_t at;
    at_command_init(&at, &mock_hal);
    ASSERT_TRUE(at.waiting == 0);
    ASSERT_TRUE(at.urc_count == 0);
    ASSERT_TRUE(strlen(at_command_get_matched(&at)) == 0);
}

TEST(test_send_command)
{
    at_command_t at;
    mock_reset();
    at_command_init(&at, &mock_hal);
    at_command_send(&at, "AT");

    ASSERT_TRUE(tx_len == 4);
    ASSERT_TRUE(memcmp(tx_buf, "AT\r\n", 4) == 0);
}

TEST(test_send_with_args)
{
    at_command_t at;
    mock_reset();
    at_command_init(&at, &mock_hal);
    at_command_send(&at, "AT+CSQ");

    ASSERT_TRUE(tx_len == 8);
    ASSERT_TRUE(memcmp(tx_buf, "AT+CSQ\r\n", 8) == 0);
}

TEST(test_ok_response)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    at_command_evt_type_t r = feed_string(&at, "OK\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_OK);
    ASSERT_TRUE(strcmp(at_command_get_matched(&at), "OK") == 0);
    ASSERT_TRUE(at.waiting == 0);
}

TEST(test_response_with_prefix)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "+CSQ:");

    at_command_evt_type_t r = feed_string(&at, "+CSQ: 18,0\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_OK);
    ASSERT_TRUE(strcmp(at_command_get_matched(&at), "+CSQ: 18,0") == 0);
}

TEST(test_error_response)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    at_command_evt_type_t r = feed_string(&at, "ERROR\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_ERROR);
    ASSERT_TRUE(at.waiting == 0);
}

TEST(test_cme_error)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    at_command_evt_type_t r = feed_string(&at, "+CME ERROR: 10\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_ERROR);
}

TEST(test_prev_line)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    /* IMEI appears on line before OK */
    feed_string(&at, "862104025396754\r\n", &evt);
    feed_string(&at, "OK\r\n", &evt);

    ASSERT_TRUE(strcmp(at_command_get_prev_line(&at), "862104025396754") == 0);
    ASSERT_TRUE(strcmp(at_command_get_matched(&at), "OK") == 0);
}

TEST(test_urc_dispatch)
{
    at_command_t at;
    at_command_evt_t evt;
    urc_reset();
    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "RDY", urc_handler, NULL);

    at_command_evt_type_t r = feed_string(&at, "RDY\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_URC);
    ASSERT_TRUE(urc_count == 1);
    ASSERT_TRUE(strcmp(urc_captured, "RDY") == 0);
}

TEST(test_urc_with_prefix)
{
    at_command_t at;
    at_command_evt_t evt;
    urc_reset();
    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "+CGREG:", urc_handler, NULL);

    feed_string(&at, "+CGREG: 0,1\r\n", &evt);
    ASSERT_TRUE(urc_count == 1);
    ASSERT_TRUE(strcmp(urc_captured, "+CGREG: 0,1") == 0);
}

TEST(test_urc_during_wait)
{
    at_command_t at;
    at_command_evt_t evt;
    urc_reset();
    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "$GP", urc_handler, NULL);
    at_command_expect(&at, "+CSQ:");

    /* NMEA URC while waiting for CSQ */
    feed_string(&at, "$GPRMC,123456,A,2345.6789,N,12345.6789,E,0.0,0.0,010126,,\r\n", &evt);
    ASSERT_TRUE(urc_count == 1);
    ASSERT_TRUE(at.waiting == 1);

    /* actual response */
    at_command_evt_type_t r = feed_string(&at, "+CSQ: 20,0\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_OK);
}

TEST(test_prompt)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, ">");

    at_command_evt_type_t r = at_command_feed(&at, '>', &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_PROMPT);
    ASSERT_TRUE(at.waiting == 0);
    ASSERT_TRUE(at_command_get_matched(&at)[0] == '>');
}

TEST(test_prompt_no_match_when_not_expected)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    at_command_evt_type_t r = at_command_feed(&at, '>', &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_NONE);
    ASSERT_TRUE(at.waiting == 1);
}

TEST(test_cancel)
{
    at_command_t at;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");
    ASSERT_TRUE(at.waiting == 1);

    at_command_cancel(&at);
    ASSERT_TRUE(at.waiting == 0);
}

TEST(test_send_raw)
{
    at_command_t at;
    mock_reset();
    at_command_init(&at, &mock_hal);

    uint8_t data[] = {0x01, 0x02, 0x03};
    at_command_send_raw(&at, data, 3);
    ASSERT_TRUE(tx_len == 3);
    ASSERT_TRUE(tx_buf[0] == 0x01 && tx_buf[2] == 0x03);
}

TEST(test_multiple_urc_handlers)
{
    at_command_t at;
    at_command_evt_t evt;
    urc_reset();

    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "RDY", urc_handler, NULL);
    at_command_register_urc(&at, "+PDP", (at_command_urc_cb_t)(void(*)(void))urc_handler, NULL);

    feed_string(&at, "RDY\r\n", &evt);
    ASSERT_TRUE(urc_count == 1);

    feed_string(&at, "+PDP DEACT\r\n", &evt);
    ASSERT_TRUE(urc_count == 2);
}

TEST(test_cr_stripping)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    at_command_evt_type_t r = feed_string(&at, "OK\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_OK);
    ASSERT_TRUE(strcmp(evt.line, "OK") == 0);
}

TEST(test_empty_lines_ignored)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    /* empty lines before response */
    feed_string(&at, "\r\n\r\n", &evt);
    ASSERT_TRUE(at.waiting == 1);

    at_command_evt_type_t r = feed_string(&at, "OK\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_OK);
}

TEST(test_null_safety)
{
    at_command_evt_t evt;
    at_command_init(NULL, &mock_hal);
    at_command_send(NULL, "AT");
    at_command_expect(NULL, "OK");
    at_command_cancel(NULL);
    ASSERT_TRUE(at_command_feed(NULL, 'A', &evt) == AT_COMMAND_EVT_NONE);
    ASSERT_TRUE(at_command_register_urc(NULL, "X", urc_handler, NULL) == -1);
    ASSERT_TRUE(strcmp(at_command_get_matched(NULL), "") == 0);
    ASSERT_TRUE(strcmp(at_command_get_prev_line(NULL), "") == 0);
}

TEST(test_cms_error)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    at_command_evt_type_t r = feed_string(&at, "+CMS ERROR: 305\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_ERROR);
}

TEST(test_bare_newline)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "+CSQ:");

    at_command_evt_type_t r = feed_string(&at, "+CSQ: 15,0\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_OK);
    ASSERT_TRUE(strcmp(at_command_get_matched(&at), "+CSQ: 15,0") == 0);
}

TEST(test_urc_table_full)
{
    at_command_t at;
    at_command_init(&at, &mock_hal);

    for (int i = 0; i < AT_COMMAND_MAX_URCS; i++) {
        ASSERT_TRUE(at_command_register_urc(&at, "X", urc_handler, NULL) == 0);
    }
    ASSERT_TRUE(at_command_register_urc(&at, "Y", urc_handler, NULL) == -1);
}

TEST(test_line_buffer_overflow)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    for (int i = 0; i < AT_COMMAND_LINE_SIZE + 100; i++) {
        at_command_feed(&at, 'A', &evt);
    }
    ASSERT_TRUE(at.line_idx == AT_COMMAND_LINE_SIZE - 1);

    at_command_evt_type_t r = at_command_feed(&at, '\n', &evt);
    ASSERT_TRUE(r != AT_COMMAND_EVT_NONE);
    ASSERT_TRUE(at.line_idx == 0);
}

TEST(test_reinit_clears_urcs)
{
    at_command_t at;
    at_command_evt_t evt;
    urc_reset();
    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "RDY", urc_handler, NULL);
    at_command_expect(&at, "OK");

    at_command_init(&at, &mock_hal);
    ASSERT_TRUE(at.urc_count == 0);
    ASSERT_TRUE(at.waiting == 0);

    feed_string(&at, "RDY\r\n", &evt);
    ASSERT_TRUE(urc_count == 0);
}

TEST(test_line_exact_boundary)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");

    for (int i = 0; i < AT_COMMAND_LINE_SIZE - 1; i++) {
        at_command_feed(&at, 'O', &evt);
    }
    ASSERT_TRUE(at.line_idx == AT_COMMAND_LINE_SIZE - 1);

    at_command_feed(&at, 'X', &evt);
    ASSERT_TRUE(at.line_idx == AT_COMMAND_LINE_SIZE - 1);
}

TEST(test_consecutive_commands)
{
    at_command_t at;
    at_command_evt_t evt;
    mock_reset();
    at_command_init(&at, &mock_hal);

    at_command_send(&at, "AT");
    at_command_expect(&at, "OK");
    feed_string(&at, "OK\r\n", &evt);
    ASSERT_TRUE(at.waiting == 0);

    at_command_send(&at, "AT+CSQ");
    at_command_expect(&at, "+CSQ:");
    at_command_evt_type_t r = feed_string(&at, "+CSQ: 20,0\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_OK);
    ASSERT_TRUE(strcmp(at_command_get_matched(&at), "+CSQ: 20,0") == 0);
}

TEST(test_register_urc_null_cb)
{
    at_command_t at;
    at_command_init(&at, &mock_hal);
    ASSERT_TRUE(at_command_register_urc(&at, "X", NULL, NULL) == -1);
}

TEST(test_register_urc_null_prefix)
{
    at_command_t at;
    at_command_init(&at, &mock_hal);
    ASSERT_TRUE(at_command_register_urc(&at, NULL, urc_handler, NULL) == -1);
}

TEST(test_send_null_cmd)
{
    at_command_t at;
    mock_reset();
    at_command_init(&at, &mock_hal);
    at_command_send(&at, NULL);
    ASSERT_TRUE(tx_len == 0);
}

TEST(test_send_raw_null)
{
    at_command_t at;
    mock_reset();
    at_command_init(&at, &mock_hal);
    at_command_send_raw(&at, NULL, 5);
    ASSERT_TRUE(tx_len == 0);
}

TEST(test_urc_not_matched_as_response)
{
    at_command_t at;
    at_command_evt_t evt;
    urc_reset();
    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "+CGREG:", urc_handler, NULL);
    at_command_expect(&at, "+CSQ:");

    feed_string(&at, "+CGREG: 1,5\r\n", &evt);
    ASSERT_TRUE(urc_count == 1);
    ASSERT_TRUE(at.waiting == 1);

    at_command_evt_type_t r = feed_string(&at, "+CSQ: 15,0\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_OK);
}

TEST(test_send_raw_zero_length)
{
    at_command_t at;
    mock_reset();
    at_command_init(&at, &mock_hal);
    at_command_send_raw(&at, (const uint8_t *)"ABC", 0);
    ASSERT_TRUE(tx_len == 0);
}

TEST(test_error_clears_matched)
{
    at_command_t at;
    at_command_evt_t evt;
    at_command_init(&at, &mock_hal);
    at_command_expect(&at, "OK");
    feed_string(&at, "OK\r\n", &evt);
    ASSERT_TRUE(strlen(at_command_get_matched(&at)) > 0);

    at_command_expect(&at, "OK");
    feed_string(&at, "ERROR\r\n", &evt);
    ASSERT_TRUE(strlen(at_command_get_matched(&at)) == 0);
    ASSERT_TRUE(at.waiting == 0);
}

TEST(test_send_appends_crlf)
{
    at_command_t at;
    mock_reset();
    at_command_init(&at, &mock_hal);
    at_command_send(&at, "AT+CSQ");
    ASSERT_TRUE(tx_len >= 8);
    ASSERT_TRUE(tx_buf[6] == '\r' && tx_buf[7] == '\n');
}

TEST(test_urc_before_any_expect)
{
    at_command_t at;
    at_command_evt_t evt;
    urc_reset();
    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "RDY", urc_handler, NULL);

    at_command_evt_type_t r = feed_string(&at, "RDY\r\n", &evt);
    ASSERT_TRUE(r == AT_COMMAND_EVT_URC);
    ASSERT_TRUE(urc_count == 1);
}

TEST(test_unregister_urc_stops_dispatch)
{
    at_command_t at;
    at_command_evt_t evt;
    urc_reset();
    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "RDY", urc_handler, NULL);

    /* fires before unregister */
    feed_string(&at, "RDY\r\n", &evt);
    ASSERT_TRUE(urc_count == 1);

    ASSERT_TRUE(at_command_unregister_urc(&at, "RDY") == 0);
    ASSERT_TRUE(at.urc_count == 0);

    /* must not fire after unregister */
    feed_string(&at, "RDY\r\n", &evt);
    ASSERT_TRUE(urc_count == 1);
}

TEST(test_unregister_urc_not_found)
{
    at_command_t at;
    at_command_init(&at, &mock_hal);
    at_command_register_urc(&at, "RDY", urc_handler, NULL);

    ASSERT_TRUE(at_command_unregister_urc(&at, "+NOSUCH") == -1);
    /* existing entry must not be disturbed */
    ASSERT_TRUE(at.urc_count == 1);
}

TEST(test_unregister_urc_null_args)
{
    at_command_t at;
    at_command_init(&at, &mock_hal);
    ASSERT_TRUE(at_command_unregister_urc(NULL, "RDY") == -1);
    ASSERT_TRUE(at_command_unregister_urc(&at, NULL) == -1);
}

TEST_MAIN("at_command")
    RUN(test_init);
    RUN(test_send_command);
    RUN(test_send_with_args);
    RUN(test_ok_response);
    RUN(test_response_with_prefix);
    RUN(test_error_response);
    RUN(test_cme_error);
    RUN(test_prev_line);
    RUN(test_urc_dispatch);
    RUN(test_urc_with_prefix);
    RUN(test_urc_during_wait);
    RUN(test_prompt);
    RUN(test_prompt_no_match_when_not_expected);
    RUN(test_cancel);
    RUN(test_send_raw);
    RUN(test_multiple_urc_handlers);
    RUN(test_cr_stripping);
    RUN(test_empty_lines_ignored);
    RUN(test_cms_error);
    RUN(test_bare_newline);
    RUN(test_null_safety);
    RUN(test_urc_table_full);
    RUN(test_line_buffer_overflow);
    RUN(test_reinit_clears_urcs);
    RUN(test_line_exact_boundary);
    RUN(test_consecutive_commands);
    RUN(test_register_urc_null_cb);
    RUN(test_register_urc_null_prefix);
    RUN(test_send_null_cmd);
    RUN(test_send_raw_null);
    RUN(test_urc_not_matched_as_response);
    RUN(test_send_raw_zero_length);
    RUN(test_error_clears_matched);
    RUN(test_send_appends_crlf);
    RUN(test_urc_before_any_expect);
    RUN(test_unregister_urc_stops_dispatch);
    RUN(test_unregister_urc_not_found);
    RUN(test_unregister_urc_null_args);
TEST_MAIN_END()

