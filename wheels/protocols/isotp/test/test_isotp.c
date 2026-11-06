// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "isotp.h"
#include <string.h>

static uint8_t rxbuf[256];
static isotp_rx_t rx;

static void setup(void)
{
    memset(rxbuf, 0, sizeof(rxbuf));
    isotp_rx_init(&rx, rxbuf, sizeof(rxbuf));
}

/* ---------- frame type ---------- */

TEST(test_frame_type)
{
    ASSERT_EQ(isotp_frame_type(0x05), ISOTP_SF);
    ASSERT_EQ(isotp_frame_type(0x10), ISOTP_FF);
    ASSERT_EQ(isotp_frame_type(0x21), ISOTP_CF);
    ASSERT_EQ(isotp_frame_type(0x30), ISOTP_FC);
}

/* ---------- single frame ---------- */

TEST(test_sf_decode)
{
    setup();
    uint8_t can[8] = {0x03, 'A', 'B', 'C', 0, 0, 0, 0};
    ASSERT_EQ(isotp_rx_feed(&rx, can, 8), ISOTP_OK);
    ASSERT_EQ(isotp_rx_length(&rx), 3u);
    ASSERT_EQ(isotp_rx_data(&rx)[0], 'A');
    ASSERT_EQ(isotp_rx_data(&rx)[2], 'C');
}

TEST(test_sf_max_7)
{
    setup();
    uint8_t can[8] = {0x07, 1, 2, 3, 4, 5, 6, 7};
    ASSERT_EQ(isotp_rx_feed(&rx, can, 8), ISOTP_OK);
    ASSERT_EQ(isotp_rx_length(&rx), 7u);
}

TEST(test_sf_zero_len)
{
    setup();
    uint8_t can[8] = {0x00, 0, 0, 0, 0, 0, 0, 0};
    ASSERT_EQ(isotp_rx_feed(&rx, can, 8), ISOTP_ERR_FORMAT);
}

/* ---------- multi-frame (FF + CF) ---------- */

TEST(test_multiframe_10bytes)
{
    setup();
    /* FF: total=10, first 6 data bytes */
    uint8_t ff[8] = {0x10, 0x0A, 0, 1, 2, 3, 4, 5};
    ASSERT_EQ(isotp_rx_feed(&rx, ff, 8), ISOTP_INCOMPLETE);
    /* CF SN=1: remaining 4 bytes */
    uint8_t cf[8] = {0x21, 6, 7, 8, 9, 0xCC, 0xCC, 0xCC};
    ASSERT_EQ(isotp_rx_feed(&rx, cf, 8), ISOTP_OK);
    ASSERT_EQ(isotp_rx_length(&rx), 10u);
    for (int i = 0; i < 10; i++)
        ASSERT_EQ(isotp_rx_data(&rx)[i], (uint8_t)i);
}

TEST(test_multiframe_20bytes)
{
    setup();
    uint8_t ff[8] = {0x10, 20, 0, 1, 2, 3, 4, 5};
    ASSERT_EQ(isotp_rx_feed(&rx, ff, 8), ISOTP_INCOMPLETE);
    uint8_t cf1[8] = {0x21, 6, 7, 8, 9, 10, 11, 12};
    ASSERT_EQ(isotp_rx_feed(&rx, cf1, 8), ISOTP_INCOMPLETE);
    uint8_t cf2[8] = {0x22, 13, 14, 15, 16, 17, 18, 19};
    ASSERT_EQ(isotp_rx_feed(&rx, cf2, 8), ISOTP_OK);
    ASSERT_EQ(isotp_rx_length(&rx), 20u);
    for (int i = 0; i < 20; i++)
        ASSERT_EQ(isotp_rx_data(&rx)[i], (uint8_t)i);
}

/* Send a whole message of len bytes as padded 8-byte frames, and check every
 * step. The bytes are 7i + 3, so a shift or a repeat shows up. */
static void feed_long_message(isotp_rx_t *r, uint16_t len)
{
    uint8_t can[8] = {0};

    can[0] = (uint8_t)(0x10 | (len >> 8));
    can[1] = (uint8_t)(len & 0xFF);
    for (uint16_t i = 0; i < 6u; i++)
        can[2 + i] = (uint8_t)(i * 7u + 3u);
    ASSERT_EQ(isotp_rx_feed(r, can, 8), ISOTP_INCOMPLETE);

    uint16_t sent = 6;
    uint8_t sn = 1;
    while (sent < len) {
        memset(can, 0xCC, sizeof(can));
        can[0] = (uint8_t)(0x20 | sn);
        for (uint16_t i = 0; i < 7u && sent < len; i++, sent++)
            can[1 + i] = (uint8_t)(sent * 7u + 3u);
        sn = (uint8_t)((sn + 1u) & 0x0F);
        ASSERT_EQ(isotp_rx_feed(r, can, 8),
                  sent < len ? ISOTP_INCOMPLETE : ISOTP_OK);
    }

    ASSERT_EQ(isotp_rx_length(r), (size_t)len);
    for (uint16_t i = 0; i < len; i++)
        ASSERT_EQ(isotp_rx_data(r)[i], (uint8_t)(i * 7u + 3u));
}

TEST(test_message_exactly_fills_the_buffer)
{
    /* 6 + 42 * 7: every frame is full, and the sequence number wraps twice. */
    uint8_t buf[300];
    isotp_rx_t r;

    ASSERT_EQ(isotp_rx_init(&r, buf, sizeof(buf)), ISOTP_OK);
    feed_long_message(&r, 300);
}

TEST(test_padding_of_the_last_frame_stays_out_of_the_message)
{
    /* 6 + 41 * 7 + 6: the last frame ends in one byte of padding. The bytes
     * after the message belong to someone else. */
    uint8_t buf[304];
    isotp_rx_t r;

    memset(buf, 0x5A, sizeof(buf));
    ASSERT_EQ(isotp_rx_init(&r, buf, 299), ISOTP_OK);
    feed_long_message(&r, 299);
    for (size_t i = 299; i < sizeof(buf); i++)
        ASSERT_EQ(buf[i], 0x5A);
}

TEST(test_first_frame_length_uses_all_12_bits)
{
    uint8_t buf[300];
    isotp_rx_t r;
    uint8_t ff[8] = {0x11, 0x2C, 0, 0, 0, 0, 0, 0};

    /* 0x12C is 300, which fits, and 0x12D does not. */
    ASSERT_EQ(isotp_rx_init(&r, buf, sizeof(buf)), ISOTP_OK);
    ASSERT_EQ(isotp_rx_feed(&r, ff, 8), ISOTP_INCOMPLETE);
    ff[1] = 0x2D;
    ASSERT_EQ(isotp_rx_feed(&r, ff, 8), ISOTP_ERR_OVERFLOW);
}

TEST(test_sf_needs_all_its_bytes_in_the_frame)
{
    setup();
    /* A length of 4 fits a DLC of 5, and a length of 5 does not. */
    uint8_t fits[5] = {0x04, 1, 2, 3, 4};
    uint8_t cut[5]  = {0x05, 1, 2, 3, 4};
    ASSERT_EQ(isotp_rx_feed(&rx, fits, 5), ISOTP_OK);
    ASSERT_EQ(isotp_rx_length(&rx), 4u);
    ASSERT_EQ(isotp_rx_feed(&rx, cut, 5), ISOTP_ERR_SHORT);
}

TEST(test_cf_wrong_sequence)
{
    setup();
    uint8_t ff[8] = {0x10, 0x0A, 0, 1, 2, 3, 4, 5};
    isotp_rx_feed(&rx, ff, 8);
    uint8_t cf[8] = {0x22, 6, 7, 8, 9, 0, 0, 0};  /* SN=2 instead of 1 */
    ASSERT_EQ(isotp_rx_feed(&rx, cf, 8), ISOTP_ERR_SEQUENCE);
}

TEST(test_wrong_sequence_ends_the_reception)
{
    setup();
    /* Message A, 20 bytes: FF and CF1 arrive, CF2 is lost. */
    uint8_t a_ff[8]  = {0x10, 20, 'A', 'A', 'A', 'A', 'A', 'A'};
    uint8_t a_cf1[8] = {0x21, 'A', 'A', 'A', 'A', 'A', 'A', 'A'};
    ASSERT_EQ(isotp_rx_feed(&rx, a_ff, 8), ISOTP_INCOMPLETE);
    ASSERT_EQ(isotp_rx_feed(&rx, a_cf1, 8), ISOTP_INCOMPLETE);

    /* Message B: its FF is lost, so its CF1 does not fit A. */
    uint8_t b_cf1[8] = {0x21, 'B', 'B', 'B', 'B', 'B', 'B', 'B'};
    uint8_t b_cf2[8] = {0x22, 'B', 'B', 'B', 'B', 'B', 'B', 'B'};
    ASSERT_EQ(isotp_rx_feed(&rx, b_cf1, 8), ISOTP_ERR_SEQUENCE);

    /* The SN of B's CF2 is what A wanted, but A has to stay dead. */
    ASSERT_EQ(isotp_rx_feed(&rx, b_cf2, 8), ISOTP_ERR_SEQUENCE);
}

TEST(test_new_message_after_wrong_sequence)
{
    setup();
    uint8_t ff[8] = {0x10, 0x0A, 0, 1, 2, 3, 4, 5};
    uint8_t bad[8] = {0x25, 9, 9, 9, 9, 9, 9, 9};
    uint8_t cf[8] = {0x21, 6, 7, 8, 9, 0xCC, 0xCC, 0xCC};
    ASSERT_EQ(isotp_rx_feed(&rx, ff, 8), ISOTP_INCOMPLETE);
    ASSERT_EQ(isotp_rx_feed(&rx, bad, 8), ISOTP_ERR_SEQUENCE);

    ASSERT_EQ(isotp_rx_feed(&rx, ff, 8), ISOTP_INCOMPLETE);
    ASSERT_EQ(isotp_rx_feed(&rx, cf, 8), ISOTP_OK);
    ASSERT_EQ(isotp_rx_length(&rx), 10u);
    for (int i = 0; i < 10; i++)
        ASSERT_EQ(isotp_rx_data(&rx)[i], (uint8_t)i);
}

TEST(test_cf_without_ff)
{
    setup();
    uint8_t cf[8] = {0x21, 1, 2, 3, 4, 5, 6, 7};
    ASSERT_EQ(isotp_rx_feed(&rx, cf, 8), ISOTP_ERR_SEQUENCE);
}

/* ---------- encode SF ---------- */

TEST(test_encode_sf)
{
    uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    uint8_t can[8] = {0};
    ASSERT_EQ(isotp_encode_sf(payload, 3, can), ISOTP_OK);
    ASSERT_EQ(can[0], 0x03);
    ASSERT_EQ(can[1], 0xAA);
    ASSERT_EQ(can[3], 0xCC);
    ASSERT_EQ(can[4], 0xCC);  /* padding */
}

TEST(test_encode_sf_too_long)
{
    uint8_t payload[8] = {0};
    uint8_t can[8];
    ASSERT_EQ(isotp_encode_sf(payload, 8, can), ISOTP_ERR_OVERFLOW);
}

TEST(test_encode_sf_null)
{
    uint8_t can[8];
    ASSERT_EQ(isotp_encode_sf(NULL, 1, can), ISOTP_ERR_PARAM);
    uint8_t p = 0;
    ASSERT_EQ(isotp_encode_sf(&p, 1, NULL), ISOTP_ERR_PARAM);
}

/* ---------- encode FC ---------- */

TEST(test_encode_fc)
{
    uint8_t can[8] = {0};
    ASSERT_EQ(isotp_encode_fc(ISOTP_FC_CTS, 0, 10, can), ISOTP_OK);
    ASSERT_EQ(can[0], 0x30);
    ASSERT_EQ(can[1], 0);
    ASSERT_EQ(can[2], 10);
}

TEST(test_encode_fc_null)
{
    ASSERT_EQ(isotp_encode_fc(ISOTP_FC_CTS, 0, 0, NULL), ISOTP_ERR_PARAM);
}

/* ---------- SF roundtrip ---------- */

TEST(test_sf_roundtrip)
{
    uint8_t payload[] = {1, 2, 3, 4, 5};
    uint8_t can[8];
    isotp_encode_sf(payload, 5, can);

    setup();
    ASSERT_EQ(isotp_rx_feed(&rx, can, 8), ISOTP_OK);
    ASSERT_EQ(isotp_rx_length(&rx), 5u);
    ASSERT_TRUE(memcmp(isotp_rx_data(&rx), payload, 5) == 0);
}

/* ---------- init / reset ---------- */

TEST(test_init_null)
{
    ASSERT_EQ(isotp_rx_init(NULL, rxbuf, 64), ISOTP_ERR_PARAM);
    ASSERT_EQ(isotp_rx_init(&rx, NULL, 64), ISOTP_ERR_PARAM);
}

TEST(test_reset)
{
    setup();
    uint8_t ff[8] = {0x10, 0x0A, 0, 1, 2, 3, 4, 5};
    isotp_rx_feed(&rx, ff, 8);
    isotp_rx_reset(&rx);
    ASSERT_EQ(isotp_rx_length(&rx), 0u);
}

TEST(test_reset_ends_the_reception)
{
    setup();
    uint8_t ff[8] = {0x10, 0x0A, 0, 1, 2, 3, 4, 5};
    uint8_t cf_next[8] = {0x21, 6, 7, 8, 9, 0xCC, 0xCC, 0xCC};
    uint8_t cf_zero[8] = {0x20, 6, 7, 8, 9, 0xCC, 0xCC, 0xCC};

    ASSERT_EQ(isotp_rx_feed(&rx, ff, 8), ISOTP_INCOMPLETE);
    ASSERT_EQ(isotp_rx_reset(&rx), ISOTP_OK);

    /* SN 1 carries on the old message, and SN 0 is what reset leaves next_sn
     * at. Neither may complete anything. */
    ASSERT_EQ(isotp_rx_feed(&rx, cf_zero, 8), ISOTP_ERR_SEQUENCE);
    ASSERT_EQ(isotp_rx_feed(&rx, cf_next, 8), ISOTP_ERR_SEQUENCE);
    ASSERT_EQ(isotp_rx_length(&rx), 0u);
}

TEST(test_reset_null)
{
    ASSERT_EQ(isotp_rx_reset(NULL), ISOTP_ERR_PARAM);
}

TEST(test_feed_null)
{
    setup();
    ASSERT_EQ(isotp_rx_feed(NULL, (uint8_t[]){0}, 1), ISOTP_ERR_PARAM);
    ASSERT_EQ(isotp_rx_feed(&rx, NULL, 1), ISOTP_ERR_PARAM);
}

TEST(test_rx_data_null)
{
    ASSERT_EQ(isotp_rx_length(NULL), 0u);
    ASSERT_TRUE(isotp_rx_data(NULL) == NULL);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)ISOTP_OK, 0);
    ASSERT_EQ((int)ISOTP_ERR_PARAM, -1);
    ASSERT_EQ((int)ISOTP_ERR_FORMAT, -2);
    ASSERT_EQ((int)ISOTP_ERR_SHORT, -3);
    ASSERT_EQ((int)ISOTP_ERR_OVERFLOW, -4);
    ASSERT_EQ((int)ISOTP_ERR_SEQUENCE, -5);
    ASSERT_EQ((int)ISOTP_INCOMPLETE, 1);
}

/* ---------- fuzz ---------- */

TEST(test_fuzz)
{
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        setup();
        uint8_t can[8];
        for (int j = 0; j < 8; j++) {
            seed = seed * 1103515245u + 12345u;
            can[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        uint8_t dlc = (uint8_t)(1 + (seed % 8));
        isotp_rx_feed(&rx, can, dlc);
    }
}

TEST(test_rx_flow_control_is_not_a_message)
{
    static const uint8_t ff[8] = { 0x10, 0x0A, 1, 2, 3, 4, 5, 6 };
    static const uint8_t fc[3] = { 0x30, 0x00, 0x00 };
    static const uint8_t cf[5] = { 0x21, 7, 8, 9, 10 };
    uint8_t buf[16];
    isotp_rx_t rx;

    ASSERT_EQ(isotp_rx_init(&rx, buf, sizeof(buf)), ISOTP_OK);

    /* With nothing received, a stray FC frame reports no message. */
    ASSERT_EQ(isotp_rx_feed(&rx, fc, sizeof(fc)), ISOTP_INCOMPLETE);

    /* In the middle of a message it reports no message either. */
    ASSERT_EQ(isotp_rx_feed(&rx, ff, sizeof(ff)), ISOTP_INCOMPLETE);
    ASSERT_EQ(isotp_rx_feed(&rx, fc, sizeof(fc)), ISOTP_INCOMPLETE);

    /* And the reassembly carries on. */
    ASSERT_EQ(isotp_rx_feed(&rx, cf, sizeof(cf)), ISOTP_OK);
    ASSERT_EQ(isotp_rx_length(&rx), 10);
}

TEST_MAIN("isotp")
    RUN(test_frame_type);
    RUN(test_sf_decode);
    RUN(test_sf_max_7);
    RUN(test_sf_zero_len);
    RUN(test_multiframe_10bytes);
    RUN(test_multiframe_20bytes);
    RUN(test_message_exactly_fills_the_buffer);
    RUN(test_padding_of_the_last_frame_stays_out_of_the_message);
    RUN(test_first_frame_length_uses_all_12_bits);
    RUN(test_sf_needs_all_its_bytes_in_the_frame);
    RUN(test_cf_wrong_sequence);
    RUN(test_wrong_sequence_ends_the_reception);
    RUN(test_new_message_after_wrong_sequence);
    RUN(test_cf_without_ff);
    RUN(test_encode_sf);
    RUN(test_encode_sf_too_long);
    RUN(test_encode_sf_null);
    RUN(test_encode_fc);
    RUN(test_encode_fc_null);
    RUN(test_sf_roundtrip);
    RUN(test_init_null);
    RUN(test_reset);
    RUN(test_reset_ends_the_reception);
    RUN(test_reset_null);
    RUN(test_feed_null);
    RUN(test_rx_data_null);
    RUN(test_error_codes);
    RUN(test_fuzz);
    RUN(test_rx_flow_control_is_not_a_message);
TEST_MAIN_END()
