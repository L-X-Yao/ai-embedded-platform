// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "slip.h"

/* ── helpers ─────────────────────────────────────────────────────── */

static void roundtrip(const uint8_t *data, uint16_t len)
{
    uint8_t enc[1024];
    uint16_t enc_len = (uint16_t)sizeof(enc);
    slip_err_t err = slip_encode(data, len, enc, &enc_len);
    ASSERT_EQ(err, SLIP_OK);

    uint8_t dec[512];
    uint16_t dec_len = (uint16_t)sizeof(dec);
    err = slip_decode(enc, enc_len, dec, &dec_len);
    ASSERT_EQ(err, SLIP_OK);
    ASSERT_EQ(dec_len, len);
    if (len > 0) {
        ASSERT_TRUE(memcmp(data, dec, len) == 0);
    }
}

/* ── encode tests ────────────────────────────────────────────────── */

TEST(test_encode_empty)
{
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    slip_err_t err = slip_encode(NULL, 0, out, &out_len);
    ASSERT_EQ(err, SLIP_OK);
    /* empty frame: END + END */
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ_HEX(out[0], SLIP_END);
    ASSERT_EQ_HEX(out[1], SLIP_END);
}

TEST(test_encode_no_special)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    slip_err_t err = slip_encode(data, 4, out, &out_len);
    ASSERT_EQ(err, SLIP_OK);
    /* END + 4 data bytes + END = 6 */
    ASSERT_EQ(out_len, 6);
    ASSERT_EQ_HEX(out[0], SLIP_END);
    ASSERT_TRUE(memcmp(out + 1, data, 4) == 0);
    ASSERT_EQ_HEX(out[5], SLIP_END);
}

TEST(test_encode_contains_end)
{
    /* 0xC0 in data must become ESC + ESC_END */
    uint8_t data[] = {0x11, 0xC0, 0x22};
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    slip_err_t err = slip_encode(data, 3, out, &out_len);
    ASSERT_EQ(err, SLIP_OK);
    /* END, 0x11, ESC, ESC_END, 0x22, END = 6 */
    ASSERT_EQ(out_len, 6);
    ASSERT_EQ_HEX(out[0], SLIP_END);
    ASSERT_EQ_HEX(out[1], 0x11);
    ASSERT_EQ_HEX(out[2], SLIP_ESC);
    ASSERT_EQ_HEX(out[3], SLIP_ESC_END);
    ASSERT_EQ_HEX(out[4], 0x22);
    ASSERT_EQ_HEX(out[5], SLIP_END);
}

TEST(test_encode_contains_esc)
{
    /* 0xDB in data must become ESC + ESC_ESC */
    uint8_t data[] = {0x11, 0xDB, 0x22};
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    slip_err_t err = slip_encode(data, 3, out, &out_len);
    ASSERT_EQ(err, SLIP_OK);
    ASSERT_EQ(out_len, 6);
    ASSERT_EQ_HEX(out[0], SLIP_END);
    ASSERT_EQ_HEX(out[1], 0x11);
    ASSERT_EQ_HEX(out[2], SLIP_ESC);
    ASSERT_EQ_HEX(out[3], SLIP_ESC_ESC);
    ASSERT_EQ_HEX(out[4], 0x22);
    ASSERT_EQ_HEX(out[5], SLIP_END);
}

TEST(test_encode_contains_both)
{
    /* data with both END and ESC bytes */
    uint8_t data[] = {0xC0, 0xDB};
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    slip_err_t err = slip_encode(data, 2, out, &out_len);
    ASSERT_EQ(err, SLIP_OK);
    /* END, ESC, ESC_END, ESC, ESC_ESC, END = 6 */
    ASSERT_EQ(out_len, 6);
    ASSERT_EQ_HEX(out[0], SLIP_END);
    ASSERT_EQ_HEX(out[1], SLIP_ESC);
    ASSERT_EQ_HEX(out[2], SLIP_ESC_END);
    ASSERT_EQ_HEX(out[3], SLIP_ESC);
    ASSERT_EQ_HEX(out[4], SLIP_ESC_ESC);
    ASSERT_EQ_HEX(out[5], SLIP_END);
}

/* ── decode / roundtrip tests ────────────────────────────────────── */

TEST(test_decode_roundtrip_empty)
{
    roundtrip(NULL, 0);
}

TEST(test_decode_roundtrip_no_special)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    roundtrip(data, (uint16_t)sizeof(data));
}

TEST(test_decode_roundtrip_with_special)
{
    uint8_t data[] = {0xC0, 0x11, 0xDB, 0x22, 0xC0, 0xDB};
    roundtrip(data, (uint16_t)sizeof(data));
}

TEST(test_decode_roundtrip_all_byte_values)
{
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = (uint8_t)i;
    }
    roundtrip(data, 256);
}

/* ── max encoded size ────────────────────────────────────────────── */

TEST(test_max_encoded_size)
{
    /* 0 bytes -> 2 (just two ENDs) */
    ASSERT_EQ(slip_max_encoded_size(0), 2);
    /* 1 byte -> worst case 4 (END + ESC + ESC_x + END) */
    ASSERT_EQ(slip_max_encoded_size(1), 4);
    /* 10 bytes -> worst case 22 */
    ASSERT_EQ(slip_max_encoded_size(10), 22);
    /* 100 bytes -> worst case 202 */
    ASSERT_EQ(slip_max_encoded_size(100), 202);
}

/* ── error handling ──────────────────────────────────────────────── */

TEST(test_encode_buffer_overflow)
{
    uint8_t data[] = {0x11, 0x22, 0x33};
    uint8_t out[2]; /* too small */
    uint16_t out_len = (uint16_t)sizeof(out);
    ASSERT_EQ(slip_encode(data, 3, out, &out_len), SLIP_ERR_OVERFLOW);
}

TEST(test_null_params_encode)
{
    uint8_t buf[16];
    uint16_t len = (uint16_t)sizeof(buf);
    uint8_t data = 0x42;

    ASSERT_EQ(slip_encode(&data, 1, NULL, &len), SLIP_ERR_PARAM);
    len = (uint16_t)sizeof(buf);
    ASSERT_EQ(slip_encode(&data, 1, buf, NULL), SLIP_ERR_PARAM);
    len = (uint16_t)sizeof(buf);
    ASSERT_EQ(slip_encode(NULL, 5, buf, &len), SLIP_ERR_PARAM);
}

TEST(test_null_params_decode)
{
    uint8_t buf[16];
    uint16_t len = (uint16_t)sizeof(buf);
    uint8_t enc[] = {0xC0, 0x11, 0xC0};

    ASSERT_EQ(slip_decode(enc, 3, NULL, &len), SLIP_ERR_PARAM);
    len = (uint16_t)sizeof(buf);
    ASSERT_EQ(slip_decode(enc, 3, buf, NULL), SLIP_ERR_PARAM);
    len = (uint16_t)sizeof(buf);
    ASSERT_EQ(slip_decode(NULL, 5, buf, &len), SLIP_ERR_PARAM);
}

TEST(test_decode_buffer_overflow)
{
    /* encode 10 bytes then try to decode into a 2-byte buffer */
    uint8_t data[10];
    memset(data, 0x55, 10);
    uint8_t enc[64];
    uint16_t enc_len = (uint16_t)sizeof(enc);
    slip_encode(data, 10, enc, &enc_len);

    uint8_t out[2];
    uint16_t out_len = (uint16_t)sizeof(out);
    ASSERT_EQ(slip_decode(enc, enc_len, out, &out_len), SLIP_ERR_OVERFLOW);
}

TEST(test_decode_invalid_esc_sequence)
{
    /* ESC followed by something other than ESC_END or ESC_ESC */
    uint8_t bad[] = {0xC0, 0x11, 0xDB, 0x42, 0xC0};
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    ASSERT_EQ(slip_decode(bad, (uint16_t)sizeof(bad), out, &out_len),
              SLIP_ERR_INVALID);
}

TEST(test_decode_esc_at_end)
{
    /* ESC at end of frame with nothing after (no trailing END) */
    uint8_t bad[] = {0xC0, 0x11, 0xDB};
    uint8_t out[16];
    uint16_t out_len = (uint16_t)sizeof(out);
    ASSERT_EQ(slip_decode(bad, (uint16_t)sizeof(bad), out, &out_len),
              SLIP_ERR_INVALID);
}

/* ── streaming decoder tests ────────────────────────────────────── */

TEST(test_stream_basic)
{
    /* encode data, then feed byte-by-byte to streaming decoder */
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t enc[16];
    uint16_t enc_len = (uint16_t)sizeof(enc);
    slip_encode(data, 3, enc, &enc_len);

    uint8_t buf[64];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    for (uint16_t i = 0; i < enc_len; i++) {
        ASSERT_EQ(slip_decoder_feed(&dec, enc[i]), SLIP_OK);
    }
    ASSERT_TRUE(slip_decoder_ready(&dec));

    const uint8_t *frame;
    uint16_t frame_len = slip_decoder_frame(&dec, &frame);
    ASSERT_EQ(frame_len, 3);
    ASSERT_TRUE(memcmp(frame, data, 3) == 0);
}

TEST(test_stream_multiple_frames)
{
    /* feed two frames sequentially */
    uint8_t data1[] = {0x11, 0x22};
    uint8_t data2[] = {0x33, 0x44, 0x55};
    uint8_t enc1[16], enc2[16];
    uint16_t enc1_len = (uint16_t)sizeof(enc1);
    uint16_t enc2_len = (uint16_t)sizeof(enc2);
    slip_encode(data1, 2, enc1, &enc1_len);
    slip_encode(data2, 3, enc2, &enc2_len);

    uint8_t buf[64];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    /* feed first frame */
    for (uint16_t i = 0; i < enc1_len; i++) {
        slip_decoder_feed(&dec, enc1[i]);
    }
    ASSERT_TRUE(slip_decoder_ready(&dec));

    const uint8_t *frame;
    uint16_t frame_len = slip_decoder_frame(&dec, &frame);
    ASSERT_EQ(frame_len, 2);
    ASSERT_TRUE(memcmp(frame, data1, 2) == 0);

    /* reset and feed second frame */
    slip_decoder_reset(&dec);
    ASSERT_FALSE(slip_decoder_ready(&dec));

    for (uint16_t i = 0; i < enc2_len; i++) {
        slip_decoder_feed(&dec, enc2[i]);
    }
    ASSERT_TRUE(slip_decoder_ready(&dec));

    frame_len = slip_decoder_frame(&dec, &frame);
    ASSERT_EQ(frame_len, 3);
    ASSERT_TRUE(memcmp(frame, data2, 3) == 0);
}

TEST(test_stream_incomplete)
{
    /* feed only partial frame — should not be ready */
    uint8_t buf[64];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    slip_decoder_feed(&dec, (uint8_t)SLIP_END);  /* leading END */
    slip_decoder_feed(&dec, 0x11);
    slip_decoder_feed(&dec, 0x22);
    /* no trailing END yet */
    ASSERT_FALSE(slip_decoder_ready(&dec));
}

TEST(test_stream_esc_at_end_invalid)
{
    /* ESC followed by END is invalid */
    uint8_t buf[64];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    slip_decoder_feed(&dec, (uint8_t)SLIP_END);  /* leading END */
    slip_decoder_feed(&dec, 0x11);
    slip_decoder_feed(&dec, (uint8_t)SLIP_ESC);
    slip_err_t err = slip_decoder_feed(&dec, (uint8_t)SLIP_END);
    ASSERT_EQ(err, SLIP_ERR_INVALID);
}

TEST(test_stream_reset)
{
    uint8_t buf[64];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    /* feed some data */
    slip_decoder_feed(&dec, (uint8_t)SLIP_END);
    slip_decoder_feed(&dec, 0x11);
    slip_decoder_feed(&dec, 0x22);

    /* reset mid-frame */
    slip_decoder_reset(&dec);
    ASSERT_FALSE(slip_decoder_ready(&dec));

    /* should accept new frame cleanly after reset */
    slip_decoder_feed(&dec, (uint8_t)SLIP_END);
    slip_decoder_feed(&dec, 0xAA);
    slip_decoder_feed(&dec, (uint8_t)SLIP_END);
    ASSERT_TRUE(slip_decoder_ready(&dec));

    const uint8_t *frame;
    uint16_t frame_len = slip_decoder_frame(&dec, &frame);
    ASSERT_EQ(frame_len, 1);
    ASSERT_EQ_HEX(frame[0], 0xAA);
}

TEST(test_stream_empty_frame)
{
    /* two consecutive ENDs with no data between = no frame (just delimiters) */
    uint8_t buf[64];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    slip_decoder_feed(&dec, (uint8_t)SLIP_END);
    slip_decoder_feed(&dec, (uint8_t)SLIP_END);
    /* consecutive ENDs with no data — no frame produced */
    ASSERT_FALSE(slip_decoder_ready(&dec));
}

TEST(test_stream_with_special_bytes)
{
    /* feed encoded frame containing END and ESC in original data */
    uint8_t data[] = {0xC0, 0xDB, 0x42};
    uint8_t enc[32];
    uint16_t enc_len = (uint16_t)sizeof(enc);
    slip_encode(data, 3, enc, &enc_len);

    uint8_t buf[64];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    for (uint16_t i = 0; i < enc_len; i++) {
        slip_decoder_feed(&dec, enc[i]);
    }
    ASSERT_TRUE(slip_decoder_ready(&dec));

    const uint8_t *frame;
    uint16_t frame_len = slip_decoder_frame(&dec, &frame);
    ASSERT_EQ(frame_len, 3);
    ASSERT_TRUE(memcmp(frame, data, 3) == 0);
}

TEST(test_stream_overflow)
{
    /* buffer too small for frame */
    uint8_t buf[2];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    slip_decoder_feed(&dec, (uint8_t)SLIP_END);
    slip_decoder_feed(&dec, 0x11);
    slip_decoder_feed(&dec, 0x22);
    slip_err_t err = slip_decoder_feed(&dec, 0x33); /* overflow */
    ASSERT_EQ(err, SLIP_ERR_OVERFLOW);
}

TEST(test_stream_init_null)
{
    uint8_t buf[8];
    ASSERT_EQ(slip_decoder_init(NULL, buf, 8), SLIP_ERR_PARAM);

    slip_decoder_t dec;
    ASSERT_EQ(slip_decoder_init(&dec, NULL, 8), SLIP_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    /* zero-init decoder struct must not crash on ready/reset */
    slip_decoder_t dec;
    memset(&dec, 0, sizeof(dec));
    ASSERT_FALSE(slip_decoder_ready(&dec));
    /* reset on zero-init should not crash (buf is NULL but pos is 0) */
    slip_decoder_reset(&dec);
}

TEST(test_encode_long_input_past_capacity)
{
    static uint8_t data[40000];
    static uint8_t out[UINT16_MAX];
    uint16_t out_len = sizeof(out);

    /* Every END byte doubles, which is 80002 bytes for a 65535-byte buffer. */
    memset(data, SLIP_END, sizeof(data));
    ASSERT_EQ(slip_encode(data, sizeof(data), out, &out_len), SLIP_ERR_OVERFLOW);
}

TEST(test_encode_long_input_that_fits)
{
    static const uint8_t data[40000];
    static uint8_t out[UINT16_MAX];
    uint16_t out_len = sizeof(out);

    /* No byte needs escaping, so this still fits the largest capacity. */
    ASSERT_EQ(slip_encode(data, sizeof(data), out, &out_len), SLIP_OK);
    ASSERT_EQ(out_len, 40002);
}

/* ── resync: SLIP has no checksum, so a broken frame must never come out ── */

static void feed_ok(slip_decoder_t *dec, const uint8_t *bytes, uint16_t n,
                    int *failures)
{
    for (uint16_t i = 0; i < n; i++) {
        if (slip_decoder_feed(dec, bytes[i]) != SLIP_OK) (*failures)++;
    }
}

TEST(test_overflow_drops_the_rest_of_the_frame)
{
    static const uint8_t head[] = { 0xC0, 1, 2, 3, 4 };
    static const uint8_t next[] = { 0x0A, 0x0B, 0xC0 };
    uint8_t buf[4];
    slip_decoder_t dec;
    int failures = 0;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    feed_ok(&dec, head, sizeof(head), &failures);
    ASSERT_EQ(failures, 0);
    ASSERT_EQ(slip_decoder_feed(&dec, 5), SLIP_ERR_OVERFLOW);
    ASSERT_EQ(slip_decoder_feed(&dec, 6), SLIP_ERR_DROPPED);
    ASSERT_EQ(slip_decoder_feed(&dec, 7), SLIP_ERR_DROPPED);
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_END), SLIP_OK);
    ASSERT_FALSE(slip_decoder_ready(&dec));

    /* That END closed the broken frame, so the next one comes through */
    feed_ok(&dec, next, sizeof(next), &failures);
    ASSERT_EQ(failures, 0);
    ASSERT_TRUE(slip_decoder_ready(&dec));
    const uint8_t *frame;
    ASSERT_EQ(slip_decoder_frame(&dec, &frame), 2);
    ASSERT_EQ_HEX(frame[0], 0x0A);
}

TEST(test_bad_escape_drops_the_rest_of_the_frame)
{
    static const uint8_t head[] = { 0xC0, 0xA1, 0xA2, 0xDB };
    static const uint8_t next[] = { 0xB1, 0xC0 };
    uint8_t buf[16];
    slip_decoder_t dec;
    int failures = 0;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    feed_ok(&dec, head, sizeof(head), &failures);
    ASSERT_EQ(failures, 0);
    ASSERT_EQ(slip_decoder_feed(&dec, 0x55), SLIP_ERR_INVALID);
    ASSERT_EQ(slip_decoder_feed(&dec, 0xA3), SLIP_ERR_DROPPED);
    ASSERT_EQ(slip_decoder_feed(&dec, 0xA4), SLIP_ERR_DROPPED);
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_END), SLIP_OK);
    ASSERT_FALSE(slip_decoder_ready(&dec));

    feed_ok(&dec, next, sizeof(next), &failures);
    ASSERT_EQ(failures, 0);
    ASSERT_TRUE(slip_decoder_ready(&dec));
    const uint8_t *frame;
    ASSERT_EQ(slip_decoder_frame(&dec, &frame), 1);
    ASSERT_EQ_HEX(frame[0], 0xB1);
}

TEST(test_byte_lost_while_a_frame_waits_breaks_the_next_frame)
{
    static const uint8_t first[] = { 0xC0, 0xA1, 0xC0 };
    static const uint8_t after[] = { 0xD1, 0xC0 };
    uint8_t buf[16];
    slip_decoder_t dec;
    int failures = 0;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    feed_ok(&dec, first, sizeof(first), &failures);
    ASSERT_EQ(failures, 0);
    ASSERT_TRUE(slip_decoder_ready(&dec));

    /* The next frame starts while the caller still holds this one */
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_END), SLIP_OK);
    ASSERT_EQ(slip_decoder_feed(&dec, 0xB1), SLIP_ERR_DROPPED);
    const uint8_t *frame;
    ASSERT_EQ(slip_decoder_frame(&dec, &frame), 1);
    ASSERT_EQ_HEX(frame[0], 0xA1);

    /* Reset does not bring B1 back: what is left of its frame must go too */
    slip_decoder_reset(&dec);
    ASSERT_EQ(slip_decoder_feed(&dec, 0xB2), SLIP_ERR_DROPPED);
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_END), SLIP_OK);
    ASSERT_FALSE(slip_decoder_ready(&dec));

    feed_ok(&dec, after, sizeof(after), &failures);
    ASSERT_EQ(failures, 0);
    ASSERT_TRUE(slip_decoder_ready(&dec));
    ASSERT_EQ(slip_decoder_frame(&dec, &frame), 1);
    ASSERT_EQ_HEX(frame[0], 0xD1);
}

TEST(test_end_while_a_frame_waits_loses_nothing)
{
    static const uint8_t first[] = { 0xC0, 0xA1, 0xC0 };
    static const uint8_t next[] = { 0xB1, 0xC0 };
    uint8_t buf[16];
    slip_decoder_t dec;
    int failures = 0;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    feed_ok(&dec, first, sizeof(first), &failures);
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_END), SLIP_OK);
    slip_decoder_reset(&dec);
    feed_ok(&dec, next, sizeof(next), &failures);
    ASSERT_EQ(failures, 0);
    ASSERT_TRUE(slip_decoder_ready(&dec));
}

TEST(test_esc_then_end_closes_the_frame_by_itself)
{
    /* The END has already arrived, so the next byte belongs to a new frame */
    static const uint8_t head[] = { 0xC0, 0x11, 0xDB };
    static const uint8_t next[] = { 0xB1, 0xC0 };
    uint8_t buf[16];
    slip_decoder_t dec;
    int failures = 0;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    feed_ok(&dec, head, sizeof(head), &failures);
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_END), SLIP_ERR_INVALID);
    feed_ok(&dec, next, sizeof(next), &failures);
    ASSERT_EQ(failures, 0);
    ASSERT_TRUE(slip_decoder_ready(&dec));
}

TEST(test_escape_inside_a_dropped_frame_is_only_dropped)
{
    uint8_t buf[1];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    ASSERT_EQ(slip_decoder_feed(&dec, 1), SLIP_OK);
    ASSERT_EQ(slip_decoder_feed(&dec, 2), SLIP_ERR_OVERFLOW);
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_ESC), SLIP_ERR_DROPPED);
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_END), SLIP_OK);
    ASSERT_EQ(slip_decoder_feed(&dec, 3), SLIP_OK);
    ASSERT_EQ(slip_decoder_feed(&dec, (uint8_t)SLIP_END), SLIP_OK);
    ASSERT_TRUE(slip_decoder_ready(&dec));
}

TEST(test_reset_does_not_end_a_drop_but_init_does)
{
    uint8_t buf[1];
    slip_decoder_t dec;
    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));

    ASSERT_EQ(slip_decoder_feed(&dec, 1), SLIP_OK);
    ASSERT_EQ(slip_decoder_feed(&dec, 2), SLIP_ERR_OVERFLOW);
    slip_decoder_reset(&dec);
    ASSERT_EQ(slip_decoder_feed(&dec, 3), SLIP_ERR_DROPPED);

    slip_decoder_init(&dec, buf, (uint16_t)sizeof(buf));
    ASSERT_EQ(slip_decoder_feed(&dec, 3), SLIP_OK);
}

TEST_MAIN("slip")
    RUN(test_encode_empty);
    RUN(test_encode_no_special);
    RUN(test_encode_contains_end);
    RUN(test_encode_contains_esc);
    RUN(test_encode_contains_both);
    RUN(test_decode_roundtrip_empty);
    RUN(test_decode_roundtrip_no_special);
    RUN(test_decode_roundtrip_with_special);
    RUN(test_decode_roundtrip_all_byte_values);
    RUN(test_max_encoded_size);
    RUN(test_encode_buffer_overflow);
    RUN(test_null_params_encode);
    RUN(test_null_params_decode);
    RUN(test_decode_buffer_overflow);
    RUN(test_decode_invalid_esc_sequence);
    RUN(test_decode_esc_at_end);
    RUN(test_stream_basic);
    RUN(test_stream_multiple_frames);
    RUN(test_stream_incomplete);
    RUN(test_stream_esc_at_end_invalid);
    RUN(test_stream_reset);
    RUN(test_stream_empty_frame);
    RUN(test_stream_with_special_bytes);
    RUN(test_stream_overflow);
    RUN(test_stream_init_null);
    RUN(test_zero_init_safety);
    RUN(test_encode_long_input_past_capacity);
    RUN(test_encode_long_input_that_fits);
    RUN(test_overflow_drops_the_rest_of_the_frame);
    RUN(test_bad_escape_drops_the_rest_of_the_frame);
    RUN(test_byte_lost_while_a_frame_waits_breaks_the_next_frame);
    RUN(test_end_while_a_frame_waits_loses_nothing);
    RUN(test_esc_then_end_closes_the_frame_by_itself);
    RUN(test_escape_inside_a_dropped_frame_is_only_dropped);
    RUN(test_reset_does_not_end_a_drop_but_init_does);
TEST_MAIN_END()
