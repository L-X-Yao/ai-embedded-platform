#include <stdio.h>
#include <string.h>
#include "../at_command.h"

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

static const at_hal_t mock_hal = {
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

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

static at_evt_type_t feed_string(at_cmd_t *at, const char *s, at_evt_t *evt)
{
    at_evt_type_t last = AT_EVT_NONE;
    while (*s) {
        at_evt_type_t r = at_cmd_feed(at, (uint8_t)*s++, evt);
        if (r != AT_EVT_NONE) {
            last = r;
        }
    }
    return last;
}

/* ---------- tests ---------- */

static void test_init(void)
{
    at_cmd_t at;

    printf("test_init\n");
    at_cmd_init(&at, &mock_hal);
    ASSERT(at.waiting == 0, "not waiting after init");
    ASSERT(at.urc_count == 0, "no URCs after init");
    ASSERT(strlen(at_cmd_get_matched(&at)) == 0, "no match after init");
}

static void test_send_command(void)
{
    at_cmd_t at;

    printf("test_send_command\n");
    mock_reset();
    at_cmd_init(&at, &mock_hal);
    at_cmd_send(&at, "AT");

    ASSERT(tx_len == 4, "4 bytes sent (AT\\r\\n)");
    ASSERT(memcmp(tx_buf, "AT\r\n", 4) == 0, "correct framing");
}

static void test_send_with_args(void)
{
    at_cmd_t at;

    printf("test_send_with_args\n");
    mock_reset();
    at_cmd_init(&at, &mock_hal);
    at_cmd_send(&at, "AT+CSQ");

    ASSERT(tx_len == 8, "8 bytes");
    ASSERT(memcmp(tx_buf, "AT+CSQ\r\n", 8) == 0, "AT+CSQ framing");
}

static void test_ok_response(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_ok_response\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");

    at_evt_type_t r = feed_string(&at, "OK\r\n", &evt);
    ASSERT(r == AT_EVT_OK, "OK event");
    ASSERT(strcmp(at_cmd_get_matched(&at), "OK") == 0, "matched line is OK");
    ASSERT(at.waiting == 0, "no longer waiting");
}

static void test_response_with_prefix(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_response_with_prefix\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "+CSQ:");

    at_evt_type_t r = feed_string(&at, "+CSQ: 18,0\r\n", &evt);
    ASSERT(r == AT_EVT_OK, "matched +CSQ:");
    ASSERT(strcmp(at_cmd_get_matched(&at), "+CSQ: 18,0") == 0, "full line captured");
}

static void test_error_response(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_error_response\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");

    at_evt_type_t r = feed_string(&at, "ERROR\r\n", &evt);
    ASSERT(r == AT_EVT_ERROR, "ERROR event");
    ASSERT(at.waiting == 0, "no longer waiting");
}

static void test_cme_error(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_cme_error\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");

    at_evt_type_t r = feed_string(&at, "+CME ERROR: 10\r\n", &evt);
    ASSERT(r == AT_EVT_ERROR, "CME ERROR detected");
}

static void test_prev_line(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_prev_line\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");

    /* IMEI appears on line before OK */
    feed_string(&at, "862104025396754\r\n", &evt);
    feed_string(&at, "OK\r\n", &evt);

    ASSERT(strcmp(at_cmd_get_prev_line(&at), "862104025396754") == 0, "prev line is IMEI");
    ASSERT(strcmp(at_cmd_get_matched(&at), "OK") == 0, "matched is OK");
}

static void test_urc_dispatch(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_urc_dispatch\n");
    urc_reset();
    at_cmd_init(&at, &mock_hal);
    at_cmd_register_urc(&at, "RDY", urc_handler, NULL);

    at_evt_type_t r = feed_string(&at, "RDY\r\n", &evt);
    ASSERT(r == AT_EVT_URC, "URC event");
    ASSERT(urc_count == 1, "handler called once");
    ASSERT(strcmp(urc_captured, "RDY") == 0, "URC line captured");
}

static void test_urc_with_prefix(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_urc_with_prefix\n");
    urc_reset();
    at_cmd_init(&at, &mock_hal);
    at_cmd_register_urc(&at, "+CGREG:", urc_handler, NULL);

    feed_string(&at, "+CGREG: 0,1\r\n", &evt);
    ASSERT(urc_count == 1, "CGREG handler called");
    ASSERT(strcmp(urc_captured, "+CGREG: 0,1") == 0, "full URC line");
}

static void test_urc_during_wait(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_urc_during_wait\n");
    urc_reset();
    at_cmd_init(&at, &mock_hal);
    at_cmd_register_urc(&at, "$GP", urc_handler, NULL);
    at_cmd_expect(&at, "+CSQ:");

    /* NMEA URC while waiting for CSQ */
    feed_string(&at, "$GPRMC,123456,A,2345.6789,N,12345.6789,E,0.0,0.0,010126,,\r\n", &evt);
    ASSERT(urc_count == 1, "URC dispatched during wait");
    ASSERT(at.waiting == 1, "still waiting for response");

    /* actual response */
    at_evt_type_t r = feed_string(&at, "+CSQ: 20,0\r\n", &evt);
    ASSERT(r == AT_EVT_OK, "response matched");
}

static void test_prompt(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_prompt\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, ">");

    at_evt_type_t r = at_cmd_feed(&at, '>', &evt);
    ASSERT(r == AT_EVT_PROMPT, "prompt event");
    ASSERT(at.waiting == 0, "no longer waiting");
    ASSERT(at_cmd_get_matched(&at)[0] == '>', "matched is >");
}

static void test_prompt_no_match_when_not_expected(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_prompt_no_match_when_not_expected\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");

    at_evt_type_t r = at_cmd_feed(&at, '>', &evt);
    ASSERT(r == AT_EVT_NONE, "no prompt match");
    ASSERT(at.waiting == 1, "still waiting");
}

static void test_cancel(void)
{
    at_cmd_t at;

    printf("test_cancel\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");
    ASSERT(at.waiting == 1, "waiting");

    at_cmd_cancel(&at);
    ASSERT(at.waiting == 0, "cancelled");
}

static void test_send_raw(void)
{
    at_cmd_t at;

    printf("test_send_raw\n");
    mock_reset();
    at_cmd_init(&at, &mock_hal);

    uint8_t data[] = {0x01, 0x02, 0x03};
    at_cmd_send_raw(&at, data, 3);
    ASSERT(tx_len == 3, "3 raw bytes");
    ASSERT(tx_buf[0] == 0x01 && tx_buf[2] == 0x03, "correct raw data");
}

static void test_multiple_urc_handlers(void)
{
    at_cmd_t at;
    at_evt_t evt;
    static char urc2_buf[64];
    static int urc2_count;

    printf("test_multiple_urc_handlers\n");
    urc_reset();
    urc2_buf[0] = '\0';
    urc2_count = 0;

    at_cmd_init(&at, &mock_hal);
    at_cmd_register_urc(&at, "RDY", urc_handler, NULL);
    at_cmd_register_urc(&at, "+PDP", (at_urc_cb_t)(void(*)(void))urc_handler, NULL);

    feed_string(&at, "RDY\r\n", &evt);
    ASSERT(urc_count == 1, "first URC handler");

    feed_string(&at, "+PDP DEACT\r\n", &evt);
    ASSERT(urc_count == 2, "second URC handler");
}

static void test_cr_stripping(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_cr_stripping\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");

    at_evt_type_t r = feed_string(&at, "OK\r\n", &evt);
    ASSERT(r == AT_EVT_OK, "OK matched");
    ASSERT(strcmp(evt.line, "OK") == 0, "CR stripped");
}

static void test_empty_lines_ignored(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_empty_lines_ignored\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");

    /* empty lines before response */
    feed_string(&at, "\r\n\r\n", &evt);
    ASSERT(at.waiting == 1, "still waiting after empty lines");

    at_evt_type_t r = feed_string(&at, "OK\r\n", &evt);
    ASSERT(r == AT_EVT_OK, "OK after empties");
}

static void test_null_safety(void)
{
    at_evt_t evt;

    printf("test_null_safety\n");
    at_cmd_init(NULL, &mock_hal);
    at_cmd_send(NULL, "AT");
    at_cmd_expect(NULL, "OK");
    at_cmd_cancel(NULL);
    ASSERT(at_cmd_feed(NULL, 'A', &evt) == AT_EVT_NONE, "null feed");
    ASSERT(at_cmd_register_urc(NULL, "X", urc_handler, NULL) == -1, "null register");
    ASSERT(strcmp(at_cmd_get_matched(NULL), "") == 0, "null get_matched");
    ASSERT(strcmp(at_cmd_get_prev_line(NULL), "") == 0, "null get_prev_line");
}

static void test_cms_error(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_cms_error\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "OK");

    at_evt_type_t r = feed_string(&at, "+CMS ERROR: 305\r\n", &evt);
    ASSERT(r == AT_EVT_ERROR, "CMS ERROR detected");
}

static void test_bare_newline(void)
{
    at_cmd_t at;
    at_evt_t evt;

    printf("test_bare_newline\n");
    at_cmd_init(&at, &mock_hal);
    at_cmd_expect(&at, "+CSQ:");

    at_evt_type_t r = feed_string(&at, "+CSQ: 15,0\n", &evt);
    ASSERT(r == AT_EVT_OK, "bare LF triggers line processing");
    ASSERT(strcmp(at_cmd_get_matched(&at), "+CSQ: 15,0") == 0, "matched without CR");
}

static void test_urc_table_full(void)
{
    at_cmd_t at;

    printf("test_urc_table_full\n");
    at_cmd_init(&at, &mock_hal);

    for (int i = 0; i < AT_CMD_MAX_URCS; i++) {
        ASSERT(at_cmd_register_urc(&at, "X", urc_handler, NULL) == 0, "register ok");
    }
    ASSERT(at_cmd_register_urc(&at, "Y", urc_handler, NULL) == -1, "table full");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_init();
    test_send_command();
    test_send_with_args();
    test_ok_response();
    test_response_with_prefix();
    test_error_response();
    test_cme_error();
    test_prev_line();
    test_urc_dispatch();
    test_urc_with_prefix();
    test_urc_during_wait();
    test_prompt();
    test_prompt_no_match_when_not_expected();
    test_cancel();
    test_send_raw();
    test_multiple_urc_handlers();
    test_cr_stripping();
    test_empty_lines_ignored();
    test_cms_error();
    test_bare_newline();
    test_null_safety();
    test_urc_table_full();

    printf("\n%d tests, %d failures\n", tests_run, tests_fail);
    return tests_fail ? 1 : 0;
}
