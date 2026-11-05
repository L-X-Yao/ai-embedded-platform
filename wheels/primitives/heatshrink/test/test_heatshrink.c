// SPDX-License-Identifier: MIT
/**
 * @file    test_heatshrink.c
 * @brief   C unit tests for heatshrink wheel.
 */

#include "test_harness.h"
#include "heatshrink.h"
#include <string.h>

/* ---- helpers ---- */

/* Compress data in one shot (all input fits in window). */
static int compress_all(const uint8_t *src, uint16_t src_len,
                        uint8_t *dst, uint16_t dst_cap,
                        uint16_t *dst_len,
                        uint8_t window_bits, uint8_t lookahead_bits)
{
    heatshrink_encoder_t enc;
    uint16_t wsize = (uint16_t)(1u << window_bits);
    uint8_t window[2048];

    if (heatshrink_encoder_init(&enc, window, wsize, lookahead_bits) != HEATSHRINK_OK) {
        return -1;
    }

    uint16_t consumed = 0;
    uint16_t total_consumed = 0;
    while (total_consumed < src_len) {
        heatshrink_sink_res_t sr = heatshrink_encoder_sink(
            &enc, &src[total_consumed],
            (uint16_t)(src_len - total_consumed), &consumed);
        total_consumed = (uint16_t)(total_consumed + consumed);

        if (sr == HEATSHRINK_SINK_FULL || consumed == 0) {
            /* Need to poll first */
            uint16_t produced = 0;
            uint16_t total_out = *dst_len;
            while (1) {
                heatshrink_poll_res_t pr = heatshrink_encoder_poll(
                    &enc, &dst[total_out],
                    (uint16_t)(dst_cap - total_out), &produced);
                total_out = (uint16_t)(total_out + produced);
                if (pr == HEATSHRINK_POLL_EMPTY) break;
            }
            *dst_len = total_out;
        }
    }

    heatshrink_encoder_finish(&enc);

    /* Poll remaining */
    uint16_t total_out = *dst_len;
    while (1) {
        uint16_t produced = 0;
        heatshrink_poll_res_t pr = heatshrink_encoder_poll(
            &enc, &dst[total_out],
            (uint16_t)(dst_cap - total_out), &produced);
        total_out = (uint16_t)(total_out + produced);
        if (pr == HEATSHRINK_POLL_EMPTY) break;
    }
    *dst_len = total_out;
    return 0;
}

/* Decompress data in one shot. */
static int decompress_all(const uint8_t *src, uint16_t src_len,
                          uint8_t *dst, uint16_t dst_cap,
                          uint16_t *dst_len,
                          uint8_t window_bits, uint8_t lookahead_bits)
{
    heatshrink_decoder_t dec;
    uint16_t wsize = (uint16_t)(1u << window_bits);
    /* Decoder needs window_size + window_size/2 */
    uint8_t buf[2048 + 1024];

    if (heatshrink_decoder_init(&dec, buf, wsize, lookahead_bits) != HEATSHRINK_OK) {
        return -1;
    }

    uint16_t consumed = 0;
    uint16_t total_consumed = 0;
    uint16_t total_out = 0;

    while (total_consumed < src_len) {
        heatshrink_decoder_sink(&dec, &src[total_consumed],
                                (uint16_t)(src_len - total_consumed), &consumed);
        total_consumed = (uint16_t)(total_consumed + consumed);

        /* Poll output */
        while (1) {
            uint16_t produced = 0;
            heatshrink_poll_res_t pr = heatshrink_decoder_poll(
                &dec, &dst[total_out],
                (uint16_t)(dst_cap - total_out), &produced);
            total_out = (uint16_t)(total_out + produced);
            if (pr == HEATSHRINK_POLL_EMPTY) break;
        }
    }

    heatshrink_decoder_finish(&dec);

    /* Drain remaining */
    while (1) {
        uint16_t produced = 0;
        heatshrink_poll_res_t pr = heatshrink_decoder_poll(
            &dec, &dst[total_out],
            (uint16_t)(dst_cap - total_out), &produced);
        total_out = (uint16_t)(total_out + produced);
        if (pr == HEATSHRINK_POLL_EMPTY) break;
    }

    *dst_len = total_out;
    return 0;
}

/* ---- Test: NULL parameter rejection ---- */

TEST(test_init_null)
{
    heatshrink_encoder_t enc;
    heatshrink_decoder_t dec;
    uint8_t buf[512];

    /* Encoder NULL checks */
    ASSERT_EQ(heatshrink_encoder_init(NULL, buf, 256, 4), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ(heatshrink_encoder_init(&enc, NULL, 256, 4), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ(heatshrink_encoder_reset(NULL), HEATSHRINK_ERR_PARAM);

    /* Bad window size (not power of 2) */
    ASSERT_EQ(heatshrink_encoder_init(&enc, buf, 300, 4), HEATSHRINK_ERR_PARAM);
    /* Window too small */
    ASSERT_EQ(heatshrink_encoder_init(&enc, buf, 128, 4), HEATSHRINK_ERR_PARAM);
    /* Window too large */
    ASSERT_EQ(heatshrink_encoder_init(&enc, buf, 4096, 4), HEATSHRINK_ERR_PARAM);
    /* Lookahead too small */
    ASSERT_EQ(heatshrink_encoder_init(&enc, buf, 256, 3), HEATSHRINK_ERR_PARAM);
    /* Lookahead too large */
    ASSERT_EQ(heatshrink_encoder_init(&enc, buf, 256, 9), HEATSHRINK_ERR_PARAM);
    /* Lookahead >= window_bits */
    ASSERT_EQ(heatshrink_encoder_init(&enc, buf, 256, 8), HEATSHRINK_ERR_PARAM);

    /* Decoder NULL checks */
    ASSERT_EQ(heatshrink_decoder_init(NULL, buf, 256, 4), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ(heatshrink_decoder_init(&dec, NULL, 256, 4), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ(heatshrink_decoder_reset(NULL), HEATSHRINK_ERR_PARAM);

    /* Sink/poll NULL checks */
    ASSERT_EQ(heatshrink_encoder_init(&enc, buf, 256, 4), HEATSHRINK_OK);
    uint16_t dummy = 0;
    ASSERT_EQ((int)heatshrink_encoder_sink(NULL, buf, 1, &dummy), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ((int)heatshrink_encoder_sink(&enc, NULL, 1, &dummy), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ((int)heatshrink_encoder_sink(&enc, buf, 1, NULL), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ((int)heatshrink_encoder_poll(NULL, buf, 1, &dummy), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ((int)heatshrink_encoder_poll(&enc, NULL, 1, &dummy), HEATSHRINK_ERR_PARAM);
    ASSERT_EQ((int)heatshrink_encoder_poll(&enc, buf, 1, NULL), HEATSHRINK_ERR_PARAM);
}

/* ---- Test: encode empty input ---- */

TEST(test_encode_empty)
{
    heatshrink_encoder_t enc;
    uint8_t window[256];
    uint8_t out[64];

    ASSERT_EQ(heatshrink_encoder_init(&enc, window, 256, 4), HEATSHRINK_OK);

    /* Finish immediately with no input */
    heatshrink_finish_res_t fr = heatshrink_encoder_finish(&enc);
    ASSERT_EQ(fr, HEATSHRINK_FINISH_DONE);

    uint16_t produced = 0;
    heatshrink_poll_res_t pr = heatshrink_encoder_poll(&enc, out, sizeof(out),
                                                       &produced);
    ASSERT_EQ(pr, HEATSHRINK_POLL_EMPTY);
    ASSERT_EQ(produced, 0);
}

/* ---- Test: round-trip simple string ---- */

TEST(test_round_trip_simple)
{
    const char *msg = "Hello, World!";
    uint16_t msg_len = (uint16_t)strlen(msg);
    uint8_t compressed[256];
    uint8_t decompressed[256];
    uint16_t comp_len = 0;
    uint16_t decomp_len = 0;

    ASSERT_EQ(compress_all((const uint8_t *)msg, msg_len,
                           compressed, sizeof(compressed), &comp_len,
                           8, 4), 0);
    ASSERT_TRUE(comp_len > 0);

    ASSERT_EQ(decompress_all(compressed, comp_len,
                             decompressed, sizeof(decompressed), &decomp_len,
                             8, 4), 0);
    ASSERT_EQ(decomp_len, msg_len);
    ASSERT_TRUE(memcmp(decompressed, msg, msg_len) == 0);
}

/* ---- Test: round-trip highly compressible data ---- */

TEST(test_round_trip_repeated)
{
    uint8_t input[256];
    /* Fill with repeating "ABABABAB..." */
    for (uint16_t i = 0; i < 256; i++) {
        input[i] = (i & 1) ? (uint8_t)'B' : (uint8_t)'A';
    }

    uint8_t compressed[512];
    uint8_t decompressed[256];
    uint16_t comp_len = 0;
    uint16_t decomp_len = 0;

    ASSERT_EQ(compress_all(input, 256, compressed, sizeof(compressed),
                           &comp_len, 8, 4), 0);

    /* Repeated data should compress well */
    ASSERT_TRUE(comp_len < 256);

    ASSERT_EQ(decompress_all(compressed, comp_len,
                             decompressed, sizeof(decompressed), &decomp_len,
                             8, 4), 0);
    ASSERT_EQ(decomp_len, 256);
    ASSERT_TRUE(memcmp(decompressed, input, 256) == 0);
}

/* ---- Test: round-trip random (incompressible) data ---- */

TEST(test_round_trip_random)
{
    /* Simple PRNG to generate pseudo-random data (deterministic) */
    uint8_t input[128];
    uint32_t seed = 0xDEADBEEFu;
    for (uint16_t i = 0; i < 128; i++) {
        seed = seed * 1103515245u + 12345u;
        input[i] = (uint8_t)(seed >> 16);
    }

    uint8_t compressed[512];
    uint8_t decompressed[128];
    uint16_t comp_len = 0;
    uint16_t decomp_len = 0;

    ASSERT_EQ(compress_all(input, 128, compressed, sizeof(compressed),
                           &comp_len, 8, 4), 0);

    /* Random data: compressed output >= input (overhead from literal flags) */
    ASSERT_TRUE(comp_len > 0);

    ASSERT_EQ(decompress_all(compressed, comp_len,
                             decompressed, sizeof(decompressed), &decomp_len,
                             8, 4), 0);
    ASSERT_EQ(decomp_len, 128);
    ASSERT_TRUE(memcmp(decompressed, input, 128) == 0);
}

/* ---- Test: streaming in small chunks ---- */

TEST(test_streaming)
{
    const char *msg = "AABABCABCDABCDE";
    uint16_t msg_len = (uint16_t)strlen(msg);

    /* Compress in 3-byte chunks */
    heatshrink_encoder_t enc;
    uint8_t enc_window[256];
    uint8_t compressed[256];
    uint16_t comp_len = 0;

    ASSERT_EQ(heatshrink_encoder_init(&enc, enc_window, 256, 4), HEATSHRINK_OK);

    uint16_t in_off = 0;
    while (in_off < msg_len) {
        uint16_t chunk = 3;
        if (in_off + chunk > msg_len) {
            chunk = (uint16_t)(msg_len - in_off);
        }
        uint16_t consumed = 0;
        heatshrink_encoder_sink(&enc, (const uint8_t *)&msg[in_off],
                                chunk, &consumed);
        in_off = (uint16_t)(in_off + consumed);

        /* Poll after each sink */
        uint16_t produced = 0;
        while (1) {
            heatshrink_poll_res_t pr = heatshrink_encoder_poll(
                &enc, &compressed[comp_len],
                (uint16_t)(sizeof(compressed) - comp_len), &produced);
            comp_len = (uint16_t)(comp_len + produced);
            if (pr == HEATSHRINK_POLL_EMPTY) break;
        }
    }

    heatshrink_encoder_finish(&enc);
    while (1) {
        uint16_t produced = 0;
        heatshrink_poll_res_t pr = heatshrink_encoder_poll(
            &enc, &compressed[comp_len],
            (uint16_t)(sizeof(compressed) - comp_len), &produced);
        comp_len = (uint16_t)(comp_len + produced);
        if (pr == HEATSHRINK_POLL_EMPTY) break;
    }

    ASSERT_TRUE(comp_len > 0);

    /* Decompress in 5-byte chunks */
    heatshrink_decoder_t dec;
    uint8_t dec_buf[256 + 128];
    uint8_t decompressed[256];
    uint16_t decomp_len = 0;

    ASSERT_EQ(heatshrink_decoder_init(&dec, dec_buf, 256, 4), HEATSHRINK_OK);

    in_off = 0;
    while (in_off < comp_len) {
        uint16_t chunk = 5;
        if (in_off + chunk > comp_len) {
            chunk = (uint16_t)(comp_len - in_off);
        }
        uint16_t consumed = 0;
        heatshrink_decoder_sink(&dec, &compressed[in_off], chunk, &consumed);
        in_off = (uint16_t)(in_off + consumed);

        uint16_t produced = 0;
        while (1) {
            heatshrink_poll_res_t pr = heatshrink_decoder_poll(
                &dec, &decompressed[decomp_len],
                (uint16_t)(sizeof(decompressed) - decomp_len), &produced);
            decomp_len = (uint16_t)(decomp_len + produced);
            if (pr == HEATSHRINK_POLL_EMPTY) break;
        }
    }

    heatshrink_decoder_finish(&dec);
    while (1) {
        uint16_t produced = 0;
        heatshrink_poll_res_t pr = heatshrink_decoder_poll(
            &dec, &decompressed[decomp_len],
            (uint16_t)(sizeof(decompressed) - decomp_len), &produced);
        decomp_len = (uint16_t)(decomp_len + produced);
        if (pr == HEATSHRINK_POLL_EMPTY) break;
    }

    ASSERT_EQ(decomp_len, msg_len);
    ASSERT_TRUE(memcmp(decompressed, msg, msg_len) == 0);
}

/* ---- Test: reset clears state ---- */

TEST(test_reset)
{
    const char *msg1 = "First message!";
    const char *msg2 = "Second test!!";
    uint16_t len1 = (uint16_t)strlen(msg1);
    uint16_t len2 = (uint16_t)strlen(msg2);

    heatshrink_encoder_t enc;
    uint8_t enc_window[256];
    ASSERT_EQ(heatshrink_encoder_init(&enc, enc_window, 256, 4), HEATSHRINK_OK);

    /* Encode first message partially */
    uint16_t consumed = 0;
    heatshrink_encoder_sink(&enc, (const uint8_t *)msg1, len1, &consumed);

    /* Reset */
    ASSERT_EQ(heatshrink_encoder_reset(&enc), HEATSHRINK_OK);

    /* Encode second message from scratch */
    uint8_t compressed[256];
    uint16_t comp_len = 0;

    ASSERT_EQ(compress_all((const uint8_t *)msg2, len2,
                           compressed, sizeof(compressed), &comp_len,
                           8, 4), 0);

    uint8_t decompressed[256];
    uint16_t decomp_len = 0;
    ASSERT_EQ(decompress_all(compressed, comp_len,
                             decompressed, sizeof(decompressed), &decomp_len,
                             8, 4), 0);
    ASSERT_EQ(decomp_len, len2);
    ASSERT_TRUE(memcmp(decompressed, msg2, len2) == 0);

    /* Decoder reset test */
    heatshrink_decoder_t dec;
    uint8_t dec_buf[256 + 128];
    ASSERT_EQ(heatshrink_decoder_init(&dec, dec_buf, 256, 4), HEATSHRINK_OK);

    /* Feed some junk then reset */
    uint8_t junk[10] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB,
                         0xFA, 0xF9, 0xF8, 0xF7, 0xF6};
    heatshrink_decoder_sink(&dec, junk, 10, &consumed);

    ASSERT_EQ(heatshrink_decoder_reset(&dec), HEATSHRINK_OK);

    /* Now decode the real compressed data */
    decomp_len = 0;
    ASSERT_EQ(decompress_all(compressed, comp_len,
                             decompressed, sizeof(decompressed), &decomp_len,
                             8, 4), 0);
    ASSERT_EQ(decomp_len, len2);
    ASSERT_TRUE(memcmp(decompressed, msg2, len2) == 0);
}

/* Compress through a small output chunk, giving up if the encoder stalls. */
static int compress_chunked(const uint8_t *src, uint16_t src_len,
                            uint8_t *dst, uint16_t dst_cap, uint16_t *dst_len,
                            uint16_t chunk_size)
{
    heatshrink_encoder_t enc;
    uint8_t window[256];
    uint8_t chunk[16];
    uint16_t in_off = 0;
    uint16_t out_off = 0;
    uint8_t finishing = 0;

    if (heatshrink_encoder_init(&enc, window, sizeof(window), 4) != HEATSHRINK_OK) {
        return -1;
    }

    while (!finishing) {
        uint16_t consumed = 0;
        uint16_t before = out_off;
        heatshrink_poll_res_t pr;

        if (in_off < src_len) {
            heatshrink_encoder_sink(&enc, &src[in_off],
                                    (uint16_t)(src_len - in_off), &consumed);
            in_off = (uint16_t)(in_off + consumed);
        }
        if (in_off == src_len) {
            heatshrink_encoder_finish(&enc);
            finishing = 1;
        }

        do {
            uint16_t produced = 0;
            pr = heatshrink_encoder_poll(&enc, chunk, chunk_size, &produced);
            if (out_off + produced > dst_cap) return -1;
            memcpy(&dst[out_off], chunk, produced);
            out_off = (uint16_t)(out_off + produced);
        } while (pr == HEATSHRINK_POLL_MORE);

        if (!finishing && consumed == 0 && out_off == before) return -1;
    }

    *dst_len = out_off;
    return 0;
}

static void fill_mixed(uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)((i / 7u) * 31u + (i % 3u));
    }
}

TEST(test_roundtrip_one_byte_output_chunks)
{
    uint8_t input[200];
    uint8_t compressed[512];
    uint8_t output[200];
    uint16_t compressed_len = 0;
    uint16_t output_len = 0;

    fill_mixed(input, sizeof(input));

    /* Every token spans a poll boundary, so it has to resume mid token. */
    ASSERT_EQ(compress_chunked(input, sizeof(input), compressed, sizeof(compressed),
                               &compressed_len, 1), 0);
    ASSERT_EQ(decompress_all(compressed, compressed_len, output, sizeof(output),
                             &output_len, 8, 4), 0);
    ASSERT_EQ(output_len, sizeof(input));
    ASSERT_MEMEQ_N(output, input, sizeof(input));
}

TEST_MAIN("heatshrink")
    RUN(test_init_null);
    RUN(test_encode_empty);
    RUN(test_round_trip_simple);
    RUN(test_round_trip_repeated);
    RUN(test_round_trip_random);
    RUN(test_streaming);
    RUN(test_reset);
    RUN(test_roundtrip_one_byte_output_chunks);
TEST_MAIN_END()
