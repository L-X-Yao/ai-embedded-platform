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
TEST_MAIN_END()
