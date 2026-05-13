// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "dmx512.h"
#include <string.h>

/* ---------- init tests ---------- */

TEST(test_init_basic)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_err_t err = dmx512_init(&ctx, buf, 512);
    ASSERT_EQ(err, DMX512_OK);
    /* Start code should be 0x00 */
    ASSERT_EQ(buf[0], DMX512_START_CODE);
    /* All channels zeroed */
    for (int i = 1; i <= 512; i++)
        ASSERT_EQ(buf[i], 0);
}

TEST(test_init_small_universe)
{
    uint8_t buf[65]; /* 1 start code + 64 channels */
    dmx512_t ctx;
    dmx512_err_t err = dmx512_init(&ctx, buf, 64);
    ASSERT_EQ(err, DMX512_OK);
}

TEST(test_init_null)
{
    uint8_t buf[16];
    dmx512_t ctx;
    ASSERT_EQ(dmx512_init(NULL, buf, 8), DMX512_ERR_PARAM);
    ASSERT_EQ(dmx512_init(&ctx, NULL, 8), DMX512_ERR_PARAM);
}

TEST(test_init_zero_channels)
{
    uint8_t buf[16];
    dmx512_t ctx;
    ASSERT_EQ(dmx512_init(&ctx, buf, 0), DMX512_ERR_RANGE);
}

TEST(test_init_too_many_channels)
{
    uint8_t buf[1024];
    dmx512_t ctx;
    ASSERT_EQ(dmx512_init(&ctx, buf, 513), DMX512_ERR_RANGE);
}

/* ---------- set/get channel tests ---------- */

TEST(test_set_get_channel)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    ASSERT_EQ(dmx512_set_channel(&ctx, 1, 255), DMX512_OK);
    ASSERT_EQ(dmx512_set_channel(&ctx, 512, 128), DMX512_OK);

    uint8_t val = 0;
    ASSERT_EQ(dmx512_get_channel(&ctx, 1, &val), DMX512_OK);
    ASSERT_EQ(val, 255);

    ASSERT_EQ(dmx512_get_channel(&ctx, 512, &val), DMX512_OK);
    ASSERT_EQ(val, 128);
}

TEST(test_set_channel_zero)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    /* Channel 0 is invalid (1-based) */
    ASSERT_EQ(dmx512_set_channel(&ctx, 0, 100), DMX512_ERR_RANGE);
}

TEST(test_set_channel_out_of_range)
{
    uint8_t buf[65];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 64);

    ASSERT_EQ(dmx512_set_channel(&ctx, 65, 100), DMX512_ERR_RANGE);
    ASSERT_EQ(dmx512_set_channel(&ctx, 64, 100), DMX512_OK);
}

TEST(test_get_channel_null)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    uint8_t val;
    ASSERT_EQ(dmx512_get_channel(NULL, 1, &val), DMX512_ERR_PARAM);
    ASSERT_EQ(dmx512_get_channel(&ctx, 1, NULL), DMX512_ERR_PARAM);
}

TEST(test_get_channel_out_of_range)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    uint8_t val;
    ASSERT_EQ(dmx512_get_channel(&ctx, 0, &val), DMX512_ERR_RANGE);
    ASSERT_EQ(dmx512_get_channel(&ctx, 513, &val), DMX512_ERR_RANGE);
}

TEST(test_set_channel_null)
{
    ASSERT_EQ(dmx512_set_channel(NULL, 1, 100), DMX512_ERR_PARAM);
}

/* ---------- set_range tests ---------- */

TEST(test_set_range)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    uint8_t values[] = {10, 20, 30, 40, 50};
    ASSERT_EQ(dmx512_set_range(&ctx, 1, values, 5), DMX512_OK);

    uint8_t val;
    for (int i = 0; i < 5; i++) {
        dmx512_get_channel(&ctx, (uint16_t)(i + 1), &val);
        ASSERT_EQ(val, values[i]);
    }
}

TEST(test_set_range_at_end)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    uint8_t values[] = {0xAA, 0xBB, 0xCC};
    ASSERT_EQ(dmx512_set_range(&ctx, 510, values, 3), DMX512_OK);

    uint8_t val;
    dmx512_get_channel(&ctx, 510, &val);
    ASSERT_EQ(val, 0xAA);
    dmx512_get_channel(&ctx, 512, &val);
    ASSERT_EQ(val, 0xCC);
}

TEST(test_set_range_overflow)
{
    uint8_t buf[65];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 64);

    uint8_t values[] = {1, 2, 3, 4, 5};
    /* Start at 62, count 5 => goes to channel 66 which overflows 64 */
    ASSERT_EQ(dmx512_set_range(&ctx, 62, values, 5), DMX512_ERR_RANGE);
}

TEST(test_set_range_null)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    ASSERT_EQ(dmx512_set_range(NULL, 1, (const uint8_t[]){1}, 1), DMX512_ERR_PARAM);
    ASSERT_EQ(dmx512_set_range(&ctx, 1, NULL, 1), DMX512_ERR_PARAM);
}

TEST(test_set_range_zero_count)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    uint8_t values[] = {1};
    ASSERT_EQ(dmx512_set_range(&ctx, 1, values, 0), DMX512_ERR_PARAM);
}

TEST(test_set_range_channel_zero)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    uint8_t values[] = {1};
    ASSERT_EQ(dmx512_set_range(&ctx, 0, values, 1), DMX512_ERR_RANGE);
}

/* ---------- get_packet tests ---------- */

TEST(test_get_packet)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    dmx512_set_channel(&ctx, 1, 0xFF);
    dmx512_set_channel(&ctx, 100, 0x42);

    const uint8_t *pkt = NULL;
    size_t pkt_len = 0;
    ASSERT_EQ(dmx512_get_packet(&ctx, &pkt, &pkt_len), DMX512_OK);
    ASSERT_NOT_NULL(pkt);
    ASSERT_EQ(pkt_len, 513);
    ASSERT_EQ(pkt[0], DMX512_START_CODE);
    ASSERT_EQ(pkt[1], 0xFF);
    ASSERT_EQ(pkt[100], 0x42);
}

TEST(test_get_packet_small_universe)
{
    uint8_t buf[11];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 10);

    const uint8_t *pkt;
    size_t pkt_len;
    ASSERT_EQ(dmx512_get_packet(&ctx, &pkt, &pkt_len), DMX512_OK);
    ASSERT_EQ(pkt_len, 11); /* start code + 10 */
}

TEST(test_get_packet_null)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    const uint8_t *pkt;
    size_t pkt_len;
    ASSERT_EQ(dmx512_get_packet(NULL, &pkt, &pkt_len), DMX512_ERR_PARAM);
    ASSERT_EQ(dmx512_get_packet(&ctx, NULL, &pkt_len), DMX512_ERR_PARAM);
    ASSERT_EQ(dmx512_get_packet(&ctx, &pkt, NULL), DMX512_ERR_PARAM);
}

/* ---------- parse_packet tests ---------- */

TEST(test_parse_packet)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    /* Build a DMX packet */
    uint8_t pkt[DMX512_PACKET_SIZE];
    pkt[0] = DMX512_START_CODE;
    for (int i = 1; i <= 512; i++)
        pkt[i] = (uint8_t)(i & 0xFF);

    ASSERT_EQ(dmx512_parse_packet(&ctx, pkt, DMX512_PACKET_SIZE), DMX512_OK);

    uint8_t val;
    dmx512_get_channel(&ctx, 1, &val);
    ASSERT_EQ(val, 1);
    dmx512_get_channel(&ctx, 256, &val);
    ASSERT_EQ(val, 0); /* 256 & 0xFF = 0 */
    dmx512_get_channel(&ctx, 512, &val);
    ASSERT_EQ(val, 0); /* 512 & 0xFF = 0 */
}

TEST(test_parse_packet_short)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    /* Set some channels first */
    dmx512_set_channel(&ctx, 10, 0xFF);

    /* Parse a packet with fewer channels than configured */
    uint8_t pkt[6] = {DMX512_START_CODE, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    ASSERT_EQ(dmx512_parse_packet(&ctx, pkt, 6), DMX512_OK);

    uint8_t val;
    dmx512_get_channel(&ctx, 1, &val);
    ASSERT_EQ(val, 0xAA);
    dmx512_get_channel(&ctx, 5, &val);
    ASSERT_EQ(val, 0xEE);
    /* Channels beyond received data should be zeroed */
    dmx512_get_channel(&ctx, 6, &val);
    ASSERT_EQ(val, 0);
    dmx512_get_channel(&ctx, 10, &val);
    ASSERT_EQ(val, 0);
}

TEST(test_parse_packet_bad_start_code)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    uint8_t pkt[] = {0x01, 0xAA, 0xBB}; /* Start code 0x01 is not standard DMX */
    ASSERT_EQ(dmx512_parse_packet(&ctx, pkt, 3), DMX512_ERR_FORMAT);
}

TEST(test_parse_packet_start_code_only)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    /* Set channel 1 first */
    dmx512_set_channel(&ctx, 1, 0xFF);

    /* Just start code, no channel data */
    uint8_t pkt[] = {DMX512_START_CODE};
    ASSERT_EQ(dmx512_parse_packet(&ctx, pkt, 1), DMX512_OK);

    /* All channels should be zeroed */
    uint8_t val;
    dmx512_get_channel(&ctx, 1, &val);
    ASSERT_EQ(val, 0);
}

TEST(test_parse_packet_null)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    ASSERT_EQ(dmx512_parse_packet(NULL, buf, 10), DMX512_ERR_PARAM);
    ASSERT_EQ(dmx512_parse_packet(&ctx, NULL, 10), DMX512_ERR_PARAM);
}

TEST(test_parse_packet_too_short)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    uint8_t pkt[] = {0x00};
    ASSERT_EQ(dmx512_parse_packet(&ctx, pkt, 0), DMX512_ERR_SHORT);
}

/* ---------- round trip ---------- */

TEST(test_roundtrip)
{
    uint8_t buf1[DMX512_PACKET_SIZE];
    uint8_t buf2[DMX512_PACKET_SIZE];
    dmx512_t tx, rx;
    dmx512_init(&tx, buf1, 512);
    dmx512_init(&rx, buf2, 512);

    /* Set up a lighting scene */
    dmx512_set_channel(&tx, 1, 255);   /* Red */
    dmx512_set_channel(&tx, 2, 128);   /* Green */
    dmx512_set_channel(&tx, 3, 64);    /* Blue */
    dmx512_set_channel(&tx, 4, 200);   /* Dimmer */

    /* Get packet and parse on receiver */
    const uint8_t *pkt;
    size_t pkt_len;
    dmx512_get_packet(&tx, &pkt, &pkt_len);
    dmx512_parse_packet(&rx, pkt, pkt_len);

    /* Verify all channels match */
    uint8_t v1, v2;
    for (uint16_t ch = 1; ch <= 512; ch++) {
        dmx512_get_channel(&tx, ch, &v1);
        dmx512_get_channel(&rx, ch, &v2);
        ASSERT_EQ(v1, v2);
    }
}

TEST(test_all_channels_full_range)
{
    uint8_t buf[DMX512_PACKET_SIZE];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 512);

    /* Set every channel to a different value */
    for (uint16_t ch = 1; ch <= 512; ch++) {
        ASSERT_EQ(dmx512_set_channel(&ctx, ch, (uint8_t)(ch & 0xFF)), DMX512_OK);
    }

    /* Read back */
    for (uint16_t ch = 1; ch <= 512; ch++) {
        uint8_t val;
        dmx512_get_channel(&ctx, ch, &val);
        ASSERT_EQ(val, (uint8_t)(ch & 0xFF));
    }
}

TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)DMX512_OK);
    ASSERT_EQ(-1, (int)DMX512_ERR_PARAM);
    ASSERT_EQ(-2, (int)DMX512_ERR_RANGE);
    ASSERT_EQ(-3, (int)DMX512_ERR_SHORT);
    ASSERT_EQ(-4, (int)DMX512_ERR_OVERFLOW);
    ASSERT_EQ(-5, (int)DMX512_ERR_FORMAT);
}

TEST(test_parse_packet_excess_data)
{
    /* Receiver has 64 channels, sender sends 512 -- only first 64 should be used */
    uint8_t buf[65];
    dmx512_t ctx;
    dmx512_init(&ctx, buf, 64);

    uint8_t pkt[DMX512_PACKET_SIZE];
    pkt[0] = DMX512_START_CODE;
    for (int i = 1; i <= 512; i++)
        pkt[i] = 0xAA;

    ASSERT_EQ(dmx512_parse_packet(&ctx, pkt, DMX512_PACKET_SIZE), DMX512_OK);

    uint8_t val;
    dmx512_get_channel(&ctx, 64, &val);
    ASSERT_EQ(val, 0xAA);
}

TEST_MAIN("dmx512")
    RUN(test_init_basic);
    RUN(test_init_small_universe);
    RUN(test_init_null);
    RUN(test_init_zero_channels);
    RUN(test_init_too_many_channels);
    RUN(test_set_get_channel);
    RUN(test_set_channel_zero);
    RUN(test_set_channel_out_of_range);
    RUN(test_get_channel_null);
    RUN(test_get_channel_out_of_range);
    RUN(test_set_channel_null);
    RUN(test_set_range);
    RUN(test_set_range_at_end);
    RUN(test_set_range_overflow);
    RUN(test_set_range_null);
    RUN(test_set_range_zero_count);
    RUN(test_set_range_channel_zero);
    RUN(test_get_packet);
    RUN(test_get_packet_small_universe);
    RUN(test_get_packet_null);
    RUN(test_parse_packet);
    RUN(test_parse_packet_short);
    RUN(test_parse_packet_bad_start_code);
    RUN(test_parse_packet_start_code_only);
    RUN(test_parse_packet_null);
    RUN(test_parse_packet_too_short);
    RUN(test_roundtrip);
    RUN(test_all_channels_full_range);
    RUN(test_error_codes);
    RUN(test_parse_packet_excess_data);
TEST_MAIN_END()
