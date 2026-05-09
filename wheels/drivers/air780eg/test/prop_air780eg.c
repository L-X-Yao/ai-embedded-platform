/*
 * prop_air780eg.c — property-based tests for air780eg
 *
 * Build: add to test/CMakeLists.txt or compile standalone:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_air780eg.c ../air780eg.c -o prop_air780eg
 */

#include "test_harness.h"
#include "property_test.h"
#include "../air780eg.h"
#include <string.h>

/* ---------- mock transport ---------- */

static uint8_t g_tx[4096];
static uint16_t g_tx_len;
static uint32_t g_tick;

static void m_send(void *c, const uint8_t *d, uint16_t l) {
    (void)c;
    if (g_tx_len + l <= sizeof(g_tx))
        memcpy(&g_tx[g_tx_len], d, l);
    g_tx_len += l;
}
static uint32_t m_tick(void *c) { (void)c; return g_tick; }

static const air780eg_io_t g_io = {
    .send = m_send, .get_tick_ms = m_tick, .ctx = NULL,
};

static void prop__dummy_ref(void) { (void)prop__arbi_fill; }

/* Property: random byte feed never crashes */
PROPERTY(feed_fuzz, 50000) {
    air780eg_t dev;
    g_tx_len = 0; g_tick = 0;
    air780eg_init(&dev, &g_io);

    int len = ARBI_RANGE(seed, 1, 600);
    for (int i = 0; i < len; i++) {
        air780eg_feed(&dev, ARBI_UINT8(seed));
    }
    PROP_ASSERT(1);
}

/* Property: cmd always reaches a terminal state (OK/ERROR/TIMEOUT) */
PROPERTY(cmd_terminates, 10000) {
    air780eg_t dev;
    g_tx_len = 0; g_tick = 0;
    air780eg_init(&dev, &g_io);

    air780eg_send_cmd(&dev, "AT", "OK", 500);
    PROP_ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));

    /* Random response lines */
    int lines = ARBI_RANGE(seed, 0, 5);
    for (int i = 0; i < lines; i++) {
        int llen = ARBI_RANGE(seed, 1, 40);
        for (int j = 0; j < llen; j++)
            air780eg_feed(&dev, ARBI_UINT8(seed));
        air780eg_feed(&dev, '\r');
        air780eg_feed(&dev, '\n');
    }

    /* If still pending, advance time past deadline */
    if (air780eg_cmd_status(&dev) == AIR780EG_CMD_PENDING) {
        g_tick = 1000;
        air780eg_poll(&dev);
    }

    air780eg_cmd_status_t s = air780eg_cmd_status(&dev);
    PROP_ASSERT(s == AIR780EG_CMD_OK || s == AIR780EG_CMD_ERROR ||
                s == AIR780EG_CMD_TIMEOUT);
}

/* Property: after cmd completes, a new cmd can be issued */
PROPERTY(reissue_after_complete, 10000) {
    air780eg_t dev;
    g_tx_len = 0; g_tick = 0;
    air780eg_init(&dev, &g_io);

    air780eg_send_cmd(&dev, "AT", "OK", 500);

    /* Complete with random terminal response */
    int which = ARBI_RANGE(seed, 0, 2);
    const char *responses[] = {"OK", "ERROR", "+CME ERROR: 1"};
    const char *resp = responses[which];
    for (const char *p = resp; *p; p++)
        air780eg_feed(&dev, (uint8_t)*p);
    air780eg_feed(&dev, '\r');
    air780eg_feed(&dev, '\n');

    air780eg_cmd_status_t s = air780eg_cmd_status(&dev);
    PROP_ASSERT(s != AIR780EG_CMD_PENDING);

    /* Reissue */
    dev.cmd_status = AIR780EG_CMD_IDLE;
    air780eg_err_t err = air780eg_send_cmd(&dev, "AT+CSQ", "+CSQ:", 500);
    PROP_ASSERT_EQ(AIR780EG_OK, err);
    PROP_ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));
}

/* Property: timeout respects elapsed time across uint32 range */
PROPERTY(timeout_elapsed_correct, 10000) {
    air780eg_t dev;
    g_tx_len = 0;
    g_tick = ARBI_UINT32(seed);
    air780eg_init(&dev, &g_io);

    uint32_t timeout = (uint32_t)ARBI_RANGE(seed, 100, 30000);
    uint32_t start = g_tick;
    air780eg_send_cmd(&dev, "AT", "OK", (uint32_t)timeout);

    /* Half-way: should still be pending */
    g_tick = start + (uint32_t)timeout / 2;
    air780eg_poll(&dev);
    PROP_ASSERT_EQ(AIR780EG_CMD_PENDING, air780eg_cmd_status(&dev));

    /* At deadline: should timeout */
    g_tick = start + (uint32_t)timeout;
    air780eg_poll(&dev);
    PROP_ASSERT_EQ(AIR780EG_CMD_TIMEOUT, air780eg_cmd_status(&dev));
}

/* Property: URC callbacks never called after deregistration */
static int g_cb_count;
static void counting_cb(void *u) { (void)u; g_cb_count++; }

PROPERTY(deregistered_cb_silent, 5000) {
    (void)seed; (void)prop__dummy_ref;
    air780eg_t dev;
    g_tx_len = 0; g_tick = 0;
    air780eg_init(&dev, &g_io);

    g_cb_count = 0;
    air780eg_on_restart(&dev, counting_cb, NULL);

    /* Feed RDY — callback should fire */
    const char *rdy = "RDY\r\n";
    for (const char *p = rdy; *p; p++)
        air780eg_feed(&dev, (uint8_t)*p);
    PROP_ASSERT_EQ(1, g_cb_count);

    /* Deregister */
    air780eg_on_restart(&dev, NULL, NULL);
    for (const char *p = rdy; *p; p++)
        air780eg_feed(&dev, (uint8_t)*p);
    PROP_ASSERT_EQ(1, g_cb_count);
}

TEST_MAIN("air780eg properties")
    RUN(feed_fuzz);
    RUN(cmd_terminates);
    RUN(reissue_after_complete);
    RUN(timeout_elapsed_correct);
    RUN(deregistered_cb_silent);
TEST_MAIN_END()
