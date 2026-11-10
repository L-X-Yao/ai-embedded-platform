// SPDX-License-Identifier: MIT
/* Built with AIR780EG_PAYLOAD_MAX 16, as a project short of RAM would. */
#include "test_harness.h"
#include <string.h>
#include "air780eg.h"

static void mock_send(void *ctx, const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)data; (void)len;
}

static uint32_t mock_tick(void *ctx)
{
    (void)ctx;
    return 0;
}

static const air780eg_io_t g_io = {
    .send = mock_send, .get_tick_ms = mock_tick, .ctx = NULL,
};

static int g_rx_count;
static uint16_t g_rx_len;
static void on_rx(void *user, const char *topic, const uint8_t *payload, uint16_t len)
{
    (void)user; (void)topic; (void)payload;
    g_rx_count++;
    g_rx_len = len;
}

static int g_restart_count;
static void on_restart(void *user) { (void)user; g_restart_count++; }

static void feed_text(air780eg_t *dev, const char *text)
{
    for (const char *p = text; *p; p++) {
        air780eg_feed(dev, (uint8_t)*p);
    }
}

TEST(test_message_that_fits_arrives)
{
    air780eg_t dev;
    g_rx_count = 0;
    air780eg_init(&dev, &g_io);
    air780eg_on_mqtt_rx(&dev, on_rx, NULL);
    feed_text(&dev, "+MSUB: \"t\",16 byte,0123456789abcdef\r\n");
    ASSERT_EQ(1, g_rx_count);
    ASSERT_EQ(16, g_rx_len);
    ASSERT_EQ(0u, air780eg_get_msg_drop_count(&dev));
}

/* Too long for this build but legal: skipped by count, so none of its bytes
 * turn into lines, and the next message is read as usual. */
TEST(test_longer_message_is_skipped_whole_and_counted)
{
    air780eg_t dev;
    g_rx_count = 0;
    g_restart_count = 0;
    air780eg_init(&dev, &g_io);
    air780eg_on_mqtt_rx(&dev, on_rx, NULL);
    air780eg_on_restart(&dev, on_restart, NULL);

    feed_text(&dev, "+MSUB: \"t\",17 byte,\r\nRDY\r\nRDY\r\nRDY\r\nx\r\n");
    ASSERT_EQ(0, g_rx_count);
    ASSERT_EQ(0, g_restart_count);
    ASSERT_EQ(1u, air780eg_get_msg_drop_count(&dev));

    feed_text(&dev, "+MSUB: \"t\",2 byte,ok\r\n");
    ASSERT_EQ(1, g_rx_count);
    ASSERT_EQ(2, g_rx_len);
}

TEST_MAIN("air780eg small payload")
    RUN(test_message_that_fits_arrives);
    RUN(test_longer_message_is_skipped_whole_and_counted);
TEST_MAIN_END()
