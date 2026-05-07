// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "sbus.h"
#include <string.h>

/* ---------- helper: build a known valid frame ---------- */

/* All channels at center (992 = 0x3E0).
 * We use encode then verify decode to avoid hand-computing bit packing. */
static void make_center_frame(uint8_t buf[SBUS_FRAME_LEN])
{
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        f.channels[i] = 992;
    }
    sbus_encode(&f, buf);
}

/*
 * Build a frame with known distinct channel values so we can verify
 * the bit packing order.  Values chosen to fit in 11 bits.
 */
static void make_distinct_frame(uint8_t buf[SBUS_FRAME_LEN], const uint16_t ch[SBUS_NUM_CHANNELS])
{
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        f.channels[i] = ch[i];
    }
    sbus_encode(&f, buf);
}

/* ---------- decode tests ---------- */

TEST(test_decode_center_frame)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);

    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ASSERT_EQ(out.channels[i], 992);
    }
    ASSERT_FALSE(out.ch17);
    ASSERT_FALSE(out.ch18);
    ASSERT_FALSE(out.frame_lost);
    ASSERT_FALSE(out.failsafe);
}

TEST(test_decode_distinct_channels)
{
    uint16_t ch[SBUS_NUM_CHANNELS];
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ch[i] = (uint16_t)(i * 128);  /* 0, 128, 256, ... 1920 */
    }

    uint8_t buf[SBUS_FRAME_LEN];
    make_distinct_frame(buf, ch);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);

    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ASSERT_EQ(out.channels[i], ch[i]);
    }
}

TEST(test_decode_all_zeros)
{
    uint16_t ch[SBUS_NUM_CHANNELS];
    memset(ch, 0, sizeof(ch));

    uint8_t buf[SBUS_FRAME_LEN];
    make_distinct_frame(buf, ch);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ASSERT_EQ(out.channels[i], 0);
    }
}

TEST(test_decode_all_max)
{
    uint16_t ch[SBUS_NUM_CHANNELS];
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ch[i] = 2047;
    }

    uint8_t buf[SBUS_FRAME_LEN];
    make_distinct_frame(buf, ch);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ASSERT_EQ(out.channels[i], 2047);
    }
}

TEST(test_decode_flags)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);

    /* Set all flags */
    buf[23] = SBUS_FLAG_CH17 | SBUS_FLAG_CH18 | SBUS_FLAG_FRAME_LOST | SBUS_FLAG_FAILSAFE;

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);
    ASSERT_TRUE(out.ch17);
    ASSERT_TRUE(out.ch18);
    ASSERT_TRUE(out.frame_lost);
    ASSERT_TRUE(out.failsafe);
}

TEST(test_decode_flag_ch17_only)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);
    buf[23] = SBUS_FLAG_CH17;

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);
    ASSERT_TRUE(out.ch17);
    ASSERT_FALSE(out.ch18);
    ASSERT_FALSE(out.frame_lost);
    ASSERT_FALSE(out.failsafe);
}

TEST(test_decode_flag_failsafe_only)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);
    buf[23] = SBUS_FLAG_FAILSAFE;

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);
    ASSERT_FALSE(out.ch17);
    ASSERT_FALSE(out.ch18);
    ASSERT_FALSE(out.frame_lost);
    ASSERT_TRUE(out.failsafe);
}

TEST(test_decode_flag_frame_lost_only)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);
    buf[23] = SBUS_FLAG_FRAME_LOST;

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);
    ASSERT_FALSE(out.ch17);
    ASSERT_FALSE(out.ch18);
    ASSERT_TRUE(out.frame_lost);
    ASSERT_FALSE(out.failsafe);
}

TEST(test_decode_bad_header)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);
    buf[0] = 0x00;  /* wrong header */

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_ERR_FORMAT);
}

TEST(test_decode_bad_footer)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);
    buf[24] = 0xFF;  /* wrong footer */

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_ERR_FORMAT);
}

TEST(test_decode_null)
{
    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(NULL, &out), SBUS_ERR_PARAM);

    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);
    ASSERT_EQ(sbus_decode(buf, NULL), SBUS_ERR_PARAM);
}

/* ---------- encode tests ---------- */

TEST(test_encode_header_footer)
{
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));

    uint8_t buf[SBUS_FRAME_LEN];
    ASSERT_EQ(sbus_encode(&f, buf), SBUS_OK);
    ASSERT_EQ(buf[0], SBUS_HEADER);
    ASSERT_EQ(buf[24], SBUS_FOOTER);
}

TEST(test_encode_flags)
{
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));
    f.ch17 = true;
    f.ch18 = true;
    f.frame_lost = true;
    f.failsafe = true;

    uint8_t buf[SBUS_FRAME_LEN];
    ASSERT_EQ(sbus_encode(&f, buf), SBUS_OK);
    ASSERT_EQ(buf[23], SBUS_FLAG_CH17 | SBUS_FLAG_CH18 |
              SBUS_FLAG_FRAME_LOST | SBUS_FLAG_FAILSAFE);
}

TEST(test_encode_null)
{
    uint8_t buf[SBUS_FRAME_LEN];
    ASSERT_EQ(sbus_encode(NULL, buf), SBUS_ERR_PARAM);

    sbus_frame_t f;
    memset(&f, 0, sizeof(f));
    ASSERT_EQ(sbus_encode(&f, NULL), SBUS_ERR_PARAM);
}

TEST(test_encode_channel_masking)
{
    /* Values above 2047 should be masked to 11 bits */
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));
    f.channels[0] = 0xFFFF;  /* should become 2047 after masking */

    uint8_t buf[SBUS_FRAME_LEN];
    ASSERT_EQ(sbus_encode(&f, buf), SBUS_OK);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);
    ASSERT_EQ(out.channels[0], 2047);
}

/* ---------- encode-decode round-trip ---------- */

TEST(test_roundtrip_center)
{
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        f.channels[i] = 992;
    }

    uint8_t buf[SBUS_FRAME_LEN];
    ASSERT_EQ(sbus_encode(&f, buf), SBUS_OK);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);

    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ASSERT_EQ(out.channels[i], 992);
    }
}

TEST(test_roundtrip_all_channels_distinct)
{
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        f.channels[i] = (uint16_t)(100 + i * 120);
    }
    f.ch17 = true;
    f.failsafe = true;

    uint8_t buf[SBUS_FRAME_LEN];
    ASSERT_EQ(sbus_encode(&f, buf), SBUS_OK);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);

    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ASSERT_EQ(out.channels[i], f.channels[i]);
    }
    ASSERT_TRUE(out.ch17);
    ASSERT_FALSE(out.ch18);
    ASSERT_FALSE(out.frame_lost);
    ASSERT_TRUE(out.failsafe);
}

TEST(test_roundtrip_boundary_values)
{
    sbus_frame_t f;
    memset(&f, 0, sizeof(f));
    f.channels[0] = 0;
    f.channels[1] = 1;
    f.channels[2] = 2046;
    f.channels[3] = 2047;
    f.channels[4] = 1024;
    f.channels[5] = 1023;

    uint8_t buf[SBUS_FRAME_LEN];
    ASSERT_EQ(sbus_encode(&f, buf), SBUS_OK);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);

    ASSERT_EQ(out.channels[0], 0);
    ASSERT_EQ(out.channels[1], 1);
    ASSERT_EQ(out.channels[2], 2046);
    ASSERT_EQ(out.channels[3], 2047);
    ASSERT_EQ(out.channels[4], 1024);
    ASSERT_EQ(out.channels[5], 1023);
}

/* ---------- byte-at-a-time parser ---------- */

TEST(test_parser_init)
{
    sbus_parser_t p;
    ASSERT_EQ(sbus_parser_init(&p), SBUS_OK);
    ASSERT_EQ(p.state, SBUS_PARSE_SEEKING);
    ASSERT_EQ(p.idx, 0);
}

TEST(test_parser_init_null)
{
    ASSERT_EQ(sbus_parser_init(NULL), SBUS_ERR_PARAM);
}

TEST(test_parser_complete_frame)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);

    sbus_parser_t p;
    sbus_parser_init(&p);

    sbus_parse_state_t st = SBUS_PARSE_SEEKING;
    for (int i = 0; i < SBUS_FRAME_LEN; i++) {
        st = sbus_parse_byte(&p, buf[i]);
    }
    ASSERT_EQ(st, SBUS_PARSE_COMPLETE);

    /* Decode from parser buffer */
    const uint8_t *pbuf = sbus_parser_get_buf(&p);
    ASSERT_TRUE(pbuf != NULL);

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(pbuf, &out), SBUS_OK);
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ASSERT_EQ(out.channels[i], 992);
    }
}

TEST(test_parser_skips_garbage_before_header)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);

    sbus_parser_t p;
    sbus_parser_init(&p);

    /* Feed garbage first */
    uint8_t garbage[] = {0xAA, 0xBB, 0xCC, 0x00, 0xFF};
    for (int i = 0; i < (int)sizeof(garbage); i++) {
        sbus_parse_state_t st = sbus_parse_byte(&p, garbage[i]);
        ASSERT_EQ(st, SBUS_PARSE_SEEKING);
    }

    /* Now feed real frame */
    sbus_parse_state_t st = SBUS_PARSE_SEEKING;
    for (int i = 0; i < SBUS_FRAME_LEN; i++) {
        st = sbus_parse_byte(&p, buf[i]);
    }
    ASSERT_EQ(st, SBUS_PARSE_COMPLETE);
}

TEST(test_parser_bad_footer_returns_error)
{
    uint8_t buf[SBUS_FRAME_LEN];
    make_center_frame(buf);
    buf[24] = 0xFF;  /* corrupt footer */

    sbus_parser_t p;
    sbus_parser_init(&p);

    sbus_parse_state_t st = SBUS_PARSE_SEEKING;
    for (int i = 0; i < SBUS_FRAME_LEN; i++) {
        st = sbus_parse_byte(&p, buf[i]);
    }
    ASSERT_EQ(st, SBUS_PARSE_ERROR);
}

TEST(test_parser_recovers_after_error)
{
    /* Send a bad frame, then a good one */
    uint8_t bad[SBUS_FRAME_LEN];
    make_center_frame(bad);
    bad[24] = 0xFF;  /* corrupt footer */

    uint8_t good[SBUS_FRAME_LEN];
    make_center_frame(good);

    sbus_parser_t p;
    sbus_parser_init(&p);

    /* Feed bad frame */
    for (int i = 0; i < SBUS_FRAME_LEN; i++) {
        sbus_parse_byte(&p, bad[i]);
    }

    /* Feed good frame */
    sbus_parse_state_t st = SBUS_PARSE_SEEKING;
    for (int i = 0; i < SBUS_FRAME_LEN; i++) {
        st = sbus_parse_byte(&p, good[i]);
    }
    ASSERT_EQ(st, SBUS_PARSE_COMPLETE);
}

TEST(test_parser_two_consecutive_frames)
{
    uint8_t buf1[SBUS_FRAME_LEN];
    uint8_t buf2[SBUS_FRAME_LEN];

    sbus_frame_t f1, f2;
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    f1.channels[0] = 100;
    f2.channels[0] = 200;
    sbus_encode(&f1, buf1);
    sbus_encode(&f2, buf2);

    sbus_parser_t p;
    sbus_parser_init(&p);

    /* Feed first frame */
    sbus_parse_state_t st = SBUS_PARSE_SEEKING;
    for (int i = 0; i < SBUS_FRAME_LEN; i++) {
        st = sbus_parse_byte(&p, buf1[i]);
    }
    ASSERT_EQ(st, SBUS_PARSE_COMPLETE);

    sbus_frame_t out;
    sbus_decode(sbus_parser_get_buf(&p), &out);
    ASSERT_EQ(out.channels[0], 100);

    /* Feed second frame -- first byte restarts parser */
    for (int i = 0; i < SBUS_FRAME_LEN; i++) {
        st = sbus_parse_byte(&p, buf2[i]);
    }
    ASSERT_EQ(st, SBUS_PARSE_COMPLETE);

    sbus_decode(sbus_parser_get_buf(&p), &out);
    ASSERT_EQ(out.channels[0], 200);
}

TEST(test_parser_null_returns_error)
{
    ASSERT_EQ(sbus_parse_byte(NULL, 0x0F), SBUS_PARSE_ERROR);
}

TEST(test_parser_get_buf_null)
{
    ASSERT_TRUE(sbus_parser_get_buf(NULL) == NULL);
}

/* ---------- channel <-> microsecond conversion ---------- */

TEST(test_channel_to_us_center)
{
    /* Center value ~992 should map near 1500 */
    uint16_t us = sbus_channel_to_us(992);
    ASSERT_TRUE(us >= 1498 && us <= 1502);
}

TEST(test_channel_to_us_min)
{
    ASSERT_EQ(sbus_channel_to_us(172), 1000);
}

TEST(test_channel_to_us_max)
{
    ASSERT_EQ(sbus_channel_to_us(1811), 2000);
}

TEST(test_channel_to_us_clamp_low)
{
    /* Below 172 should clamp to 1000 */
    ASSERT_EQ(sbus_channel_to_us(0), 1000);
    ASSERT_EQ(sbus_channel_to_us(100), 1000);
}

TEST(test_channel_to_us_clamp_high)
{
    /* Above 1811 should clamp to 2000 */
    ASSERT_EQ(sbus_channel_to_us(2000), 2000);
    ASSERT_EQ(sbus_channel_to_us(2047), 2000);
}

TEST(test_us_to_channel_min)
{
    ASSERT_EQ(sbus_us_to_channel(1000), 172);
}

TEST(test_us_to_channel_max)
{
    ASSERT_EQ(sbus_us_to_channel(2000), 1811);
}

TEST(test_us_to_channel_center)
{
    uint16_t ch = sbus_us_to_channel(1500);
    /* Should be near 992 */
    ASSERT_TRUE(ch >= 990 && ch <= 993);
}

TEST(test_us_to_channel_clamp_low)
{
    ASSERT_EQ(sbus_us_to_channel(500), 172);
    ASSERT_EQ(sbus_us_to_channel(0), 172);
}

TEST(test_us_to_channel_clamp_high)
{
    ASSERT_EQ(sbus_us_to_channel(2500), 1811);
    ASSERT_EQ(sbus_us_to_channel(3000), 1811);
}

TEST(test_us_roundtrip)
{
    /* channel -> us -> channel should be close */
    for (uint16_t ch = 172; ch <= 1811; ch += 50) {
        uint16_t us = sbus_channel_to_us(ch);
        uint16_t back = sbus_us_to_channel(us);
        /* Allow +-1 for integer rounding */
        int diff = (int)back - (int)ch;
        ASSERT_TRUE(diff >= -2 && diff <= 2);
    }
}

/* ---------- known frame from real receiver ---------- */

/*
 * Manually constructed frame: channels 1-16 set to specific values.
 * CH1=1000, all others=0.  CH1 = bits[0:10] in bytes[1:2].
 * 1000 = 0x3E8 = 0b01111101000
 * byte1 = lower 8 bits = 0xE8
 * byte2 bits[0:2] = upper 3 bits of CH1 = 0b011 = 0x03
 * Since CH2=0, byte2 remaining bits = 0, so byte2 = 0x03.
 */
TEST(test_decode_known_ch1)
{
    uint8_t frame[SBUS_FRAME_LEN];
    memset(frame, 0, SBUS_FRAME_LEN);
    frame[0] = SBUS_HEADER;
    frame[24] = SBUS_FOOTER;

    /* CH1 = 1000 = 0x3E8, packed in bits [0:10] of payload */
    frame[1] = 0xE8;       /* bits 0-7 of CH1 */
    frame[2] = 0x03;       /* bits 8-10 of CH1 (0b011), rest zero (CH2 starts at bit 11) */

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(frame, &out), SBUS_OK);
    ASSERT_EQ(out.channels[0], 1000);
    ASSERT_EQ(out.channels[1], 0);
}

/* ---------- adversarial / fuzz ---------- */

TEST(test_adversarial_all_ff)
{
    uint8_t buf[SBUS_FRAME_LEN];
    memset(buf, 0xFF, sizeof(buf));

    sbus_frame_t out;
    /* Bad header -> format error */
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_ERR_FORMAT);
}

TEST(test_adversarial_valid_header_footer_ff_payload)
{
    uint8_t buf[SBUS_FRAME_LEN];
    memset(buf, 0xFF, sizeof(buf));
    buf[0] = SBUS_HEADER;
    buf[24] = SBUS_FOOTER;

    sbus_frame_t out;
    ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);
    /* All channels should be 2047 (all bits set) */
    for (int i = 0; i < SBUS_NUM_CHANNELS; i++) {
        ASSERT_EQ(out.channels[i], 2047);
    }
    /* All flags set */
    ASSERT_TRUE(out.ch17);
    ASSERT_TRUE(out.ch18);
    ASSERT_TRUE(out.frame_lost);
    ASSERT_TRUE(out.failsafe);
}

TEST(test_mini_fuzz_encode_decode)
{
    unsigned seed = 42;
    for (int i = 0; i < 500; i++) {
        sbus_frame_t f;
        memset(&f, 0, sizeof(f));
        for (int ch = 0; ch < SBUS_NUM_CHANNELS; ch++) {
            seed = seed * 1103515245u + 12345u;
            f.channels[ch] = (uint16_t)((seed >> 16) % 2048);
        }
        seed = seed * 1103515245u + 12345u;
        f.ch17       = (seed & 1) != 0;
        f.ch18       = (seed & 2) != 0;
        f.frame_lost = (seed & 4) != 0;
        f.failsafe   = (seed & 8) != 0;

        uint8_t buf[SBUS_FRAME_LEN];
        ASSERT_EQ(sbus_encode(&f, buf), SBUS_OK);

        sbus_frame_t out;
        ASSERT_EQ(sbus_decode(buf, &out), SBUS_OK);

        for (int ch = 0; ch < SBUS_NUM_CHANNELS; ch++) {
            ASSERT_EQ(out.channels[ch], f.channels[ch]);
        }
        ASSERT_EQ(out.ch17, f.ch17);
        ASSERT_EQ(out.ch18, f.ch18);
        ASSERT_EQ(out.frame_lost, f.frame_lost);
        ASSERT_EQ(out.failsafe, f.failsafe);
    }
}

TEST(test_mini_fuzz_parser)
{
    sbus_parser_t p;
    sbus_parser_init(&p);

    unsigned seed = 99;
    for (int i = 0; i < 2000; i++) {
        seed = seed * 1103515245u + 12345u;
        uint8_t byte = (uint8_t)(seed >> 16);
        sbus_parse_byte(&p, byte);
    }
    /* Must not crash -- state is indeterminate but valid */
    ASSERT_TRUE(p.state >= SBUS_PARSE_SEEKING && p.state <= SBUS_PARSE_ERROR);
}

/* ---------- error code values ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)SBUS_OK, 0);
    ASSERT_EQ((int)SBUS_ERR_PARAM, -1);
    ASSERT_EQ((int)SBUS_ERR_FORMAT, -2);
}

/* ---------- constants ---------- */

TEST(test_constants)
{
    ASSERT_EQ(SBUS_NUM_CHANNELS, 16);
    ASSERT_EQ(SBUS_FRAME_LEN, 25);
    ASSERT_EQ(SBUS_HEADER, 0x0F);
    ASSERT_EQ(SBUS_FOOTER, 0x00);
}

/* ---------- main ---------- */

TEST_MAIN("sbus")
    /* decode */
    RUN(test_decode_center_frame);
    RUN(test_decode_distinct_channels);
    RUN(test_decode_all_zeros);
    RUN(test_decode_all_max);
    RUN(test_decode_flags);
    RUN(test_decode_flag_ch17_only);
    RUN(test_decode_flag_failsafe_only);
    RUN(test_decode_flag_frame_lost_only);
    RUN(test_decode_bad_header);
    RUN(test_decode_bad_footer);
    RUN(test_decode_null);
    RUN(test_decode_known_ch1);
    /* encode */
    RUN(test_encode_header_footer);
    RUN(test_encode_flags);
    RUN(test_encode_null);
    RUN(test_encode_channel_masking);
    /* round-trip */
    RUN(test_roundtrip_center);
    RUN(test_roundtrip_all_channels_distinct);
    RUN(test_roundtrip_boundary_values);
    /* parser */
    RUN(test_parser_init);
    RUN(test_parser_init_null);
    RUN(test_parser_complete_frame);
    RUN(test_parser_skips_garbage_before_header);
    RUN(test_parser_bad_footer_returns_error);
    RUN(test_parser_recovers_after_error);
    RUN(test_parser_two_consecutive_frames);
    RUN(test_parser_null_returns_error);
    RUN(test_parser_get_buf_null);
    /* channel <-> us conversion */
    RUN(test_channel_to_us_center);
    RUN(test_channel_to_us_min);
    RUN(test_channel_to_us_max);
    RUN(test_channel_to_us_clamp_low);
    RUN(test_channel_to_us_clamp_high);
    RUN(test_us_to_channel_min);
    RUN(test_us_to_channel_max);
    RUN(test_us_to_channel_center);
    RUN(test_us_to_channel_clamp_low);
    RUN(test_us_to_channel_clamp_high);
    RUN(test_us_roundtrip);
    /* adversarial */
    RUN(test_adversarial_all_ff);
    RUN(test_adversarial_valid_header_footer_ff_payload);
    RUN(test_mini_fuzz_encode_decode);
    RUN(test_mini_fuzz_parser);
    /* meta */
    RUN(test_error_codes);
    RUN(test_constants);
TEST_MAIN_END()
