#include <stdio.h>
#include <string.h>
#include "../ncp5623_parser.h"

static int tests_run, tests_fail;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        tests_fail++; \
    } \
} while(0)

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

static void test_init(void)
{
    ncp5623_parser_t p;

    printf("test_init\n");
    ncp5623_parser_init(&p);
    ASSERT(p.idx == 0, "idx zero");
    ASSERT(p.enabled == 0, "disabled after init");
}

static void test_enable(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_enable\n");
    ncp5623_parser_init(&p);

    uint8_t data[] = {0x04, 0x03};
    ncp5623_evt_type_t r = send_transaction(&p, data, 2, &evt);
    ASSERT(r == NCP5623_EVT_ENABLE, "enable event");
    ASSERT(evt.data.enabled == 1, "enabled=1");
    ASSERT(p.enabled == 1, "parser state enabled");
}

static void test_disable(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_disable\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);
    ASSERT(p.enabled == 1, "enabled first");

    uint8_t disable[] = {0x04, 0x00};
    ncp5623_evt_type_t r = send_transaction(&p, disable, 2, &evt);
    ASSERT(r == NCP5623_EVT_ENABLE, "disable event");
    ASSERT(evt.data.enabled == 0, "enabled=0");
    ASSERT(p.enabled == 0, "parser state disabled");
}

static void test_color_after_enable(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_color_after_enable\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    /* COLOR: reg=0x01, B=0x0F, G=0x00, R=0x00 → pure blue */
    uint8_t color[] = {0x01, 0x0F, 0x00, 0x00};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT(r == NCP5623_EVT_COLOR, "color event");
    ASSERT(evt.data.color.b == 0x0F, "blue=15");
    ASSERT(evt.data.color.g == 0x00, "green=0");
    ASSERT(evt.data.color.r == 0x00, "red=0");
}

static void test_color_before_enable_ignored(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_color_before_enable_ignored\n");
    ncp5623_parser_init(&p);

    uint8_t color[] = {0x01, 0x0F, 0x0F, 0x0F};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT(r == NCP5623_EVT_NONE, "no event when disabled");
}

static void test_color_red(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_color_red\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t color[] = {0x01, 0x00, 0x00, 0x0F};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT(r == NCP5623_EVT_COLOR, "color event");
    ASSERT(evt.data.color.r == 0x0F, "red=15");
    ASSERT(evt.data.color.g == 0x00, "green=0");
    ASSERT(evt.data.color.b == 0x00, "blue=0");
}

static void test_color_yellow(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_color_yellow\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t color[] = {0x01, 0x00, 0x08, 0x08};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT(r == NCP5623_EVT_COLOR, "color event");
    ASSERT(evt.data.color.r == 0x08, "red=8");
    ASSERT(evt.data.color.g == 0x08, "green=8");
    ASSERT(evt.data.color.b == 0x00, "blue=0");
}

static void test_4bit_masking(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_4bit_masking\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    /* upper nibble should be masked out */
    uint8_t color[] = {0x01, 0xF5, 0xA3, 0x71};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT(r == NCP5623_EVT_COLOR, "color event");
    ASSERT(evt.data.color.b == 0x05, "blue masked to 5");
    ASSERT(evt.data.color.g == 0x03, "green masked to 3");
    ASSERT(evt.data.color.r == 0x01, "red masked to 1");
}

static void test_unknown_register_ignored(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_unknown_register_ignored\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t unknown[] = {0x07, 0xAA, 0xBB, 0xCC};
    ncp5623_evt_type_t r = send_transaction(&p, unknown, 4, &evt);
    ASSERT(r == NCP5623_EVT_NONE, "unknown register ignored");
}

static void test_enable_persists_across_transactions(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_enable_persists_across_transactions\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    /* multiple color transactions should all work */
    uint8_t c1[] = {0x01, 0x0F, 0x00, 0x00};
    uint8_t c2[] = {0x01, 0x00, 0x0F, 0x00};
    uint8_t c3[] = {0x01, 0x00, 0x00, 0x0F};

    ASSERT(send_transaction(&p, c1, 4, &evt) == NCP5623_EVT_COLOR, "color 1");
    ASSERT(evt.data.color.b == 0x0F, "c1 blue");

    ASSERT(send_transaction(&p, c2, 4, &evt) == NCP5623_EVT_COLOR, "color 2");
    ASSERT(evt.data.color.g == 0x0F, "c2 green");

    ASSERT(send_transaction(&p, c3, 4, &evt) == NCP5623_EVT_COLOR, "color 3");
    ASSERT(evt.data.color.r == 0x0F, "c3 red");
}

static void test_short_color_packet_ignored(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_short_color_packet_ignored\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    /* only 2 data bytes instead of 3 */
    uint8_t short_color[] = {0x01, 0x0F, 0x0F};
    ncp5623_evt_type_t r = send_transaction(&p, short_color, 3, &evt);
    ASSERT(r == NCP5623_EVT_NONE, "short packet ignored");
}

static void test_null_safety(void)
{
    ncp5623_evt_t evt;

    printf("test_null_safety\n");
    ncp5623_parser_init(NULL);
    ncp5623_parser_start(NULL);
    ncp5623_parser_stop(NULL);
    ncp5623_evt_type_t r = ncp5623_parser_feed(NULL, 0x04, &evt);
    ASSERT(r == NCP5623_EVT_NONE, "null parser safe");

    ncp5623_parser_t p;
    ncp5623_parser_init(&p);
    r = ncp5623_parser_feed(&p, 0x04, NULL);
    ASSERT(r == NCP5623_EVT_NONE, "null evt safe");
}

static void test_off_color(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_off_color\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t off[] = {0x01, 0x00, 0x00, 0x00};
    ncp5623_evt_type_t r = send_transaction(&p, off, 4, &evt);
    ASSERT(r == NCP5623_EVT_COLOR, "off color event");
    ASSERT(evt.data.color.r == 0, "r=0");
    ASSERT(evt.data.color.g == 0, "g=0");
    ASSERT(evt.data.color.b == 0, "b=0");
}

static void test_reinit_clears_enabled(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_reinit_clears_enabled\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);
    ASSERT(p.enabled == 1, "enabled before re-init");

    ncp5623_parser_init(&p);
    ASSERT(p.enabled == 0, "disabled after re-init");
    ASSERT(p.idx == 0, "idx reset after re-init");

    uint8_t color[] = {0x01, 0x0F, 0x00, 0x00};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT(r == NCP5623_EVT_NONE, "color ignored after re-init (not enabled)");
}

static void test_empty_transaction(void)
{
    ncp5623_parser_t p;

    printf("test_empty_transaction\n");
    ncp5623_parser_init(&p);

    ncp5623_parser_start(&p);
    ncp5623_parser_stop(&p);
    ASSERT(p.idx == 0, "idx stays 0 after empty transaction");
    ASSERT(p.enabled == 0, "still disabled after empty transaction");
}

static void test_extra_long_packet(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_extra_long_packet\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t long_pkt[] = {0x01, 0x0F, 0x05, 0x0A, 0xFF, 0xFF, 0xFF};
    ncp5623_evt_type_t r = send_transaction(&p, long_pkt, 7, &evt);
    ASSERT(r == NCP5623_EVT_COLOR, "color parsed from overlong packet");
    ASSERT(evt.data.color.b == 0x0F, "blue from extra-long");
    ASSERT(evt.data.color.g == 0x05, "green from extra-long");
    ASSERT(evt.data.color.r == 0x0A, "red from extra-long");
}

static void test_disable_then_color_ignored(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_disable_then_color_ignored\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);
    ASSERT(p.enabled == 1, "enabled");

    uint8_t disable[] = {0x04, 0x00};
    send_transaction(&p, disable, 2, NULL);
    ASSERT(p.enabled == 0, "disabled");

    uint8_t color[] = {0x01, 0x0F, 0x0F, 0x0F};
    ncp5623_evt_type_t r = send_transaction(&p, color, 4, &evt);
    ASSERT(r == NCP5623_EVT_NONE, "color ignored after disable");
}

static void test_rapid_enable_disable_enable(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_rapid_enable_disable_enable\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, &evt);
    ASSERT(p.enabled == 1, "first enable");

    uint8_t disable[] = {0x04, 0x00};
    send_transaction(&p, disable, 2, &evt);
    ASSERT(p.enabled == 0, "disabled");

    ncp5623_evt_type_t r = send_transaction(&p, enable, 2, &evt);
    ASSERT(r == NCP5623_EVT_ENABLE, "re-enable event");
    ASSERT(p.enabled == 1, "re-enabled");
    ASSERT(evt.data.enabled == 1, "re-enable data correct");

    /* color should work after re-enable */
    uint8_t color[] = {0x01, 0x05, 0x0A, 0x0F};
    r = send_transaction(&p, color, 4, &evt);
    ASSERT(r == NCP5623_EVT_COLOR, "color after re-enable");
    ASSERT(evt.data.color.b == 0x05, "blue ok");
}

static void test_enable_non_standard_value(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_enable_non_standard_value\n");
    ncp5623_parser_init(&p);

    uint8_t enable_0x01[] = {0x04, 0x01};
    ncp5623_evt_type_t r = send_transaction(&p, enable_0x01, 2, &evt);
    ASSERT(r == NCP5623_EVT_ENABLE, "event fired for 0x01");
    ASSERT(evt.data.enabled == 0, "0x01 does not enable");
    ASSERT(p.enabled == 0, "parser stays disabled");

    uint8_t enable_0xff[] = {0x04, 0xFF};
    r = send_transaction(&p, enable_0xff, 2, &evt);
    ASSERT(r == NCP5623_EVT_ENABLE, "event fired for 0xFF");
    ASSERT(evt.data.enabled == 0, "0xFF does not enable");
}

static void test_register_address_boundaries(void)
{
    ncp5623_parser_t p;
    ncp5623_evt_t evt;

    printf("test_register_address_boundaries\n");
    ncp5623_parser_init(&p);

    uint8_t enable[] = {0x04, 0x03};
    send_transaction(&p, enable, 2, NULL);

    uint8_t reg00[] = {0x00, 0x0F, 0x0F, 0x0F};
    ASSERT(send_transaction(&p, reg00, 4, &evt) == NCP5623_EVT_NONE,
           "reg 0x00 ignored");

    uint8_t reg02[] = {0x02, 0x0F, 0x0F, 0x0F};
    ASSERT(send_transaction(&p, reg02, 4, &evt) == NCP5623_EVT_NONE,
           "reg 0x02 ignored");

    uint8_t reg03[] = {0x03, 0xFF};
    ASSERT(send_transaction(&p, reg03, 2, &evt) == NCP5623_EVT_NONE,
           "reg 0x03 ignored");

    uint8_t reg05[] = {0x05, 0xAA, 0xBB};
    ASSERT(send_transaction(&p, reg05, 3, &evt) == NCP5623_EVT_NONE,
           "reg 0x05 ignored");

    uint8_t regFF[] = {0xFF, 0x00, 0x00, 0x00};
    ASSERT(send_transaction(&p, regFF, 4, &evt) == NCP5623_EVT_NONE,
           "reg 0xFF ignored");
}

int main(void)
{
    tests_run = tests_fail = 0;

    test_init();
    test_enable();
    test_disable();
    test_color_after_enable();
    test_color_before_enable_ignored();
    test_color_red();
    test_color_yellow();
    test_4bit_masking();
    test_unknown_register_ignored();
    test_enable_persists_across_transactions();
    test_short_color_packet_ignored();
    test_null_safety();
    test_off_color();
    test_reinit_clears_enabled();
    test_empty_transaction();
    test_extra_long_packet();
    test_disable_then_color_ignored();
    test_rapid_enable_disable_enable();
    test_enable_non_standard_value();
    test_register_address_boundaries();

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_fail, tests_fail);
    return tests_fail ? 1 : 0;
}
