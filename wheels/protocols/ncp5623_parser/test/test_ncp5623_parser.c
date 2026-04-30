#include "test_harness.h"
#include <string.h>
#include "ncp5623_parser.h"
/* helper: simulate a full I2C transaction */
static ncp5623_evt_type_t send_transaction(ncp5623_parser_t *p,
                                            const uint8_t *data,
                                            int len,
                                            ncp5623_evt_t *evt_out)
{
    ncp5623_evt_type_t last = NCP5623_EVT_NONE;
    ncp5623_evt_t evt;

    ncp5623_parser_start(p);
    for (int i = 0; i < len; i++) {
        ncp5623_evt_type_t r = ncp5623_parser_feed(p, data[i], &evt);
        if (r != NCP5623_EVT_NONE) {
            last = r;
            if (evt_out) *evt_out = evt;
        }
    }
    ncp5623_parser_stop(p);
    return last;
}

TEST(test_init)
{
    ncp5623_parser_t p;
    ncp5623_parser_init(&p);
    ASSERT_TRUE(p.idx == 0);
    ASSERT_TRUE(p.enabled == 0);
}

TEST(test_enable)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t data[] = {0x04, 0x03};
    ncp5623_evt_type_t r = send_transaction(&p, data, 2, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_ENABLE);
    ASSERT_TRUE(evt.data.enabled == 1);
    ASSERT_TRUE(p.enabled == 1);
}

TEST(test_disable)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);
    ASSERT_TRUE(p.enabled == 1);

    uint8_t disable[] = {0x04, 0x00};
    ncp5623_evt_type_t r = send_transaction(&p, disable, 2, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_ENABLE);
    ASSERT_TRUE(evt.data.enabled == 0);
    ASSERT_TRUE(p.enabled == 0);
}

TEST(test_color_after_enable)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    /* COLOR: reg=0x01, B=0x0F, G=0x00, R=0x00 → pure blue */
    uint8_t color[] = {0x01, 0x0F, 0x00, 0x00};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.b == 0x0F);
    ASSERT_TRUE(evt.data.color.g == 0x00);
    ASSERT_TRUE(evt.data.color.r == 0x00);
}

TEST(test_color_before_enable_ignored)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t color[] = {0x01, 0x0F, 0x0F, 0x0F};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_NONE);
}

TEST(test_color_red)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t color[] = {0x01, 0x00, 0x00, 0x0F};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.r == 0x0F);
    ASSERT_TRUE(evt.data.color.g == 0x00);
    ASSERT_TRUE(evt.data.color.b == 0x00);
}

TEST(test_color_yellow)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t color[] = {0x01, 0x00, 0x08, 0x08};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.r == 0x08);
    ASSERT_TRUE(evt.data.color.g == 0x08);
    ASSERT_TRUE(evt.data.color.b == 0x00);
}

TEST(test_4bit_masking)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    /* upper nibble should be masked out */
    uint8_t color[] = {0x01, 0xF5, 0xA3, 0x71};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.b == 0x05);
    ASSERT_TRUE(evt.data.color.g == 0x03);
    ASSERT_TRUE(evt.data.color.r == 0x01);
}

TEST(test_unknown_register_ignored)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t unknown[] = {0x07, 0xAA, 0xBB, 0xCC};
    ncp5623_evt_type_t r = send_transaction(&p, unknown, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_NONE);
}

TEST(test_enable_persists_across_transactions)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    /* multiple color transactions should all work */
    uint8_t c1[] = {0x01, 0x0F, 0x00, 0x00};
    uint8_t c2[] = {0x01, 0x00, 0x0F, 0x00};
    uint8_t c3[] = {0x01, 0x00, 0x00, 0x0F};

    ASSERT_TRUE(send_transaction(&p, c1, 4, &evt) == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.b == 0x0F);

    ASSERT_TRUE(send_transaction(&p, c2, 4, &evt) == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.g == 0x0F);

    ASSERT_TRUE(send_transaction(&p, c3, 4, &evt) == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.r == 0x0F);
}

TEST(test_short_color_packet_ignored)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    /* only 2 data bytes instead of 3 */
    uint8_t short_color[] = {0x01, 0x0F, 0x0F};
    ncp5623_evt_type_t r = send_transaction(&p, short_color, 3, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_NONE);
}

TEST(test_null_safety)
{
    ncp5623_evt_t evt;
    ncp5623_parser_init(NULL);
    ncp5623_parser_start(NULL);
    ncp5623_parser_stop(NULL);
    ncp5623_evt_type_t r = ncp5623_parser_feed(NULL, 0x04, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_NONE);

    ncp5623_parser_t p;
    ncp5623_parser_init(&p);
    r = ncp5623_parser_feed(&p, 0x04, NULL);
    ASSERT_TRUE(r == NCP5623_EVT_NONE);
}

TEST(test_off_color)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t off[] = {0x01, 0x00, 0x00, 0x00};
    ncp5623_evt_type_t r = send_transaction(&p, off, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.r == 0);
    ASSERT_TRUE(evt.data.color.g == 0);
    ASSERT_TRUE(evt.data.color.b == 0);
}

TEST(test_reinit_clears_enabled)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);
    ASSERT_TRUE(p.enabled == 1);

    ncp5623_parser_init(&p);
    ASSERT_TRUE(p.enabled == 0);
    ASSERT_TRUE(p.idx == 0);

    uint8_t color[] = {0x01, 0x0F, 0x00, 0x00};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_NONE);
}

TEST(test_empty_transaction)
{
    ncp5623_parser_t p;
    ncp5623_parser_init(&p);

    ncp5623_parser_start(&p);
    ncp5623_parser_stop(&p);
    ASSERT_TRUE(p.idx == 0);
    ASSERT_TRUE(p.enabled == 0);
}

TEST(test_extra_long_packet)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t long_pkt[] = {0x01, 0x0F, 0x05, 0x0A, 0xFF, 0xFF, 0xFF};
    ncp5623_evt_type_t r = send_transaction(&p, long_pkt, 7, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.b == 0x0F);
    ASSERT_TRUE(evt.data.color.g == 0x05);
    ASSERT_TRUE(evt.data.color.r == 0x0A);
}

TEST(test_disable_then_color_ignored)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);
    ASSERT_TRUE(p.enabled == 1);

    uint8_t disable[] = {0x04, 0x00};
    send_transaction(&p, disable, 2, NULL);
    ASSERT_TRUE(p.enabled == 0);

    uint8_t color[] = {0x01, 0x0F, 0x0F, 0x0F};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_NONE);
}

TEST(test_rapid_enable_disable_enable)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, &evt);
    ASSERT_TRUE(p.enabled == 1);

    uint8_t disable[] = {0x04, 0x00};
    send_transaction(&p, disable, 2, &evt);
    ASSERT_TRUE(p.enabled == 0);

    ncp5623_evt_type_t r = send_transaction(&p, enable, 2, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_ENABLE);
    ASSERT_TRUE(p.enabled == 1);
    ASSERT_TRUE(evt.data.enabled == 1);

    /* color should work after re-enable */
    uint8_t color[] = {0x01, 0x05, 0x0A, 0x0F};
    r = send_transaction(&p, color, 4, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_COLOR);
    ASSERT_TRUE(evt.data.color.b == 0x05);
}

TEST(test_enable_non_standard_value)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable_0x01[] = {0x04, 0x01};
    ncp5623_evt_type_t r = send_transaction(&p, enable_0x01, 2, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_ENABLE);
    ASSERT_TRUE(evt.data.enabled == 0);
    ASSERT_TRUE(p.enabled == 0);

    uint8_t enable_0xff[] = {0x04, 0xFF};
    r = send_transaction(&p, enable_0xff, 2, &evt);
    ASSERT_TRUE(r == NCP5623_EVT_ENABLE);
    ASSERT_TRUE(evt.data.enabled == 0);
}

TEST(test_register_address_boundaries)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t reg00[] = {0x00, 0x0F, 0x0F, 0x0F};
    ASSERT_TRUE(send_transaction(&p, reg00, 4, &evt) == NCP5623_EVT_NONE);

    uint8_t reg02[] = {0x02, 0x0F, 0x0F, 0x0F};
    ASSERT_TRUE(send_transaction(&p, reg02, 4, &evt) == NCP5623_EVT_NONE);

    uint8_t reg03[] = {0x03, 0xFF};
    ASSERT_TRUE(send_transaction(&p, reg03, 2, &evt) == NCP5623_EVT_NONE);

    uint8_t reg05[] = {0x05, 0xAA, 0xBB};
    ASSERT_TRUE(send_transaction(&p, reg05, 3, &evt) == NCP5623_EVT_NONE);

    uint8_t regFF[] = {0xFF, 0x00, 0x00, 0x00};
    ASSERT_TRUE(send_transaction(&p, regFF, 4, &evt) == NCP5623_EVT_NONE);
}

TEST_MAIN("ncp5623_parser")
    RUN(test_init);
    RUN(test_enable);
    RUN(test_disable);
    RUN(test_color_after_enable);
    RUN(test_color_before_enable_ignored);
    RUN(test_color_red);
    RUN(test_color_yellow);
    RUN(test_4bit_masking);
    RUN(test_unknown_register_ignored);
    RUN(test_enable_persists_across_transactions);
    RUN(test_short_color_packet_ignored);
    RUN(test_null_safety);
    RUN(test_off_color);
    RUN(test_reinit_clears_enabled);
    RUN(test_empty_transaction);
    RUN(test_extra_long_packet);
    RUN(test_disable_then_color_ignored);
    RUN(test_rapid_enable_disable_enable);
    RUN(test_enable_non_standard_value);
    RUN(test_register_address_boundaries);
TEST_MAIN_END()

