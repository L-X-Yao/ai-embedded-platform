/*
 * prop_at_command.c — property-based tests for at_command
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_at_command.c ../at_command.c -o prop_at_command
 */

#include "test_harness.h"
#include "property_test.h"
#include "../at_command.h"
#include <string.h>

/* Capture buffer for HAL send callback */
static uint8_t g_sent[512];
static uint16_t g_sent_len;

static void mock_send(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx;
    if (g_sent_len + len <= sizeof(g_sent)) {
        memcpy(g_sent + g_sent_len, data, len);
        g_sent_len += len;
    }
}

static const at_command_hal_t mock_hal = {
    .send = mock_send,
    .ctx  = NULL,
};

/* Property: send then feed "OK\r\n" produces EVT_OK */
PROPERTY(send_ok_roundtrip, 10000) {
    at_command_t at;
    at_command_init(&at, &mock_hal);

    /* Build a random AT command: AT+XXX where XXX is 1..10 uppercase */
    char cmd[20] = "AT+";
    uint8_t clen = (uint8_t)ARBI_RANGE(seed, 1, 10);
    for (uint8_t i = 0; i < clen; i++)
        cmd[3 + i] = (char)ARBI_RANGE(seed, 'A', 'Z');
    cmd[3 + clen] = '\0';

    g_sent_len = 0;
    at_command_send(&at, cmd);
    at_command_expect(&at, "OK");

    /* sent buffer should contain cmd + \r\n */
    size_t expected_len = strlen(cmd) + 2;
    PROP_ASSERT_EQ(g_sent_len, expected_len);

    /* Feed back "OK\r\n" byte by byte */
    const char *ok_resp = "OK\r\n";
    at_command_evt_t evt;
    at_command_evt_type_t last = AT_COMMAND_EVT_NONE;
    for (size_t i = 0; i < 4; i++) {
        last = at_command_feed(&at, (uint8_t)ok_resp[i], &evt);
    }
    PROP_ASSERT_EQ(last, AT_COMMAND_EVT_OK);
}

/* Property: feed arbitrary bytes never crashes */
PROPERTY(feed_no_crash, 50000) {
    (void)prop__seed_orig; (void)prop__iter;
    at_command_t at;
    at_command_init(&at, &mock_hal);

    uint8_t garbage[128];
    uint8_t len = (uint8_t)ARBI_RANGE(seed, 1, 128);
    ARBI_FILL(seed, garbage, len);

    at_command_evt_t evt;
    for (uint8_t i = 0; i < len; i++) {
        at_command_feed(&at, garbage[i], &evt);
    }
}

/* Property: send is deterministic — same command produces same bytes */
PROPERTY(send_determinism, 5000) {
    char cmd[16] = "AT+";
    uint8_t clen = (uint8_t)ARBI_RANGE(seed, 1, 8);
    for (uint8_t i = 0; i < clen; i++)
        cmd[3 + i] = (char)ARBI_RANGE(seed, 'A', 'Z');
    cmd[3 + clen] = '\0';

    at_command_t at;

    g_sent_len = 0;
    at_command_init(&at, &mock_hal);
    at_command_send(&at, cmd);
    uint8_t buf1[256];
    uint16_t len1 = g_sent_len;
    memcpy(buf1, g_sent, len1);

    g_sent_len = 0;
    at_command_init(&at, &mock_hal);
    at_command_send(&at, cmd);
    PROP_ASSERT_EQ(g_sent_len, len1);
    PROP_ASSERT_MEM_EQ(buf1, g_sent, len1);
}

/* Property: URC registration and feed triggers callback */
static int g_urc_count;
static void urc_cb(const char *line, void *ctx)
{
    (void)line;
    (void)ctx;
    g_urc_count++;
}

PROPERTY(urc_detection, 5000) {
    at_command_t at;
    at_command_init(&at, &mock_hal);
    g_urc_count = 0;

    int rc = at_command_register_urc(&at, "+CREG", urc_cb, NULL);
    PROP_ASSERT_EQ(rc, 0);

    /* Generate a URC line: +CREG: X\r\n where X is random digit */
    char urc_line[32];
    int ulen = snprintf(urc_line, sizeof(urc_line),
                        "+CREG: %d\r\n", (int)ARBI_RANGE(seed, 0, 9));
    PROP_ASSERT(ulen > 0);

    at_command_evt_t evt;
    for (int i = 0; i < ulen; i++) {
        at_command_feed(&at, (uint8_t)urc_line[i], &evt);
    }
    PROP_ASSERT(g_urc_count >= 1);
}

TEST_MAIN("at_command properties") {
    RUN(send_ok_roundtrip);
    RUN(feed_no_crash);
    RUN(send_determinism);
    RUN(urc_detection);
}
TEST_MAIN_END()
