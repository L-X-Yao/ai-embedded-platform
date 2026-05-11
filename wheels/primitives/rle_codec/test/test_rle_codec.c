// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "rle_codec.h"

/* ── encode: basic ──────────────────────────────────────────────── */

TEST(test_encode_single_byte)
{
    uint8_t src[] = {0xAA};
    uint8_t dst[4];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 1, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ(dst[0], 1);    /* count */
    ASSERT_EQ(dst[1], 0xAA); /* value */
}

TEST(test_encode_run_of_three)
{
    uint8_t src[] = {0xBB, 0xBB, 0xBB};
    uint8_t dst[4];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 3, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ(dst[0], 3);
    ASSERT_EQ(dst[1], 0xBB);
}

TEST(test_encode_mixed)
{
    uint8_t src[] = {1, 1, 1, 2, 2, 3};
    uint8_t dst[16];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 6, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 6); /* 3 pairs: [3,1] [2,2] [1,3] */
    ASSERT_EQ(dst[0], 3); ASSERT_EQ(dst[1], 1);
    ASSERT_EQ(dst[2], 2); ASSERT_EQ(dst[3], 2);
    ASSERT_EQ(dst[4], 1); ASSERT_EQ(dst[5], 3);
}

TEST(test_encode_all_different)
{
    uint8_t src[] = {10, 20, 30, 40};
    uint8_t dst[16];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 4, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 8); /* 4 pairs of [1, val] */
    ASSERT_EQ(dst[0], 1); ASSERT_EQ(dst[1], 10);
    ASSERT_EQ(dst[2], 1); ASSERT_EQ(dst[3], 20);
    ASSERT_EQ(dst[4], 1); ASSERT_EQ(dst[5], 30);
    ASSERT_EQ(dst[6], 1); ASSERT_EQ(dst[7], 40);
}

TEST(test_encode_empty)
{
    uint8_t dst[4];
    size_t out_len = 99;
    ASSERT_EQ(rle_codec_encode((const uint8_t *)"", 0, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 0);
}

/* ── encode: long run split at 255 ──────────────────────────────── */

TEST(test_encode_run_split_255)
{
    uint8_t src[300];
    memset(src, 0xCC, 300);
    uint8_t dst[8];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 300, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    /* 300 = 255 + 45 → 2 pairs */
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ(dst[0], 255);  ASSERT_EQ(dst[1], 0xCC);
    ASSERT_EQ(dst[2], 45);   ASSERT_EQ(dst[3], 0xCC);
}

TEST(test_encode_exactly_255)
{
    uint8_t src[255];
    memset(src, 0xDD, 255);
    uint8_t dst[4];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 255, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ(dst[0], 255);
    ASSERT_EQ(dst[1], 0xDD);
}

TEST(test_encode_256_bytes)
{
    uint8_t src[256];
    memset(src, 0xEE, 256);
    uint8_t dst[8];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 256, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    /* 256 = 255 + 1 → 2 pairs */
    ASSERT_EQ(out_len, 4);
    ASSERT_EQ(dst[0], 255); ASSERT_EQ(dst[1], 0xEE);
    ASSERT_EQ(dst[2], 1);   ASSERT_EQ(dst[3], 0xEE);
}

/* ── encode: overflow ───────────────────────────────────────────── */

TEST(test_encode_overflow_exact)
{
    uint8_t src[] = {1, 2};
    uint8_t dst[3]; /* need 4 but only 3 */
    size_t out_len = 99;
    ASSERT_EQ(rle_codec_encode(src, 2, dst, 3, &out_len), RLE_CODEC_ERR_OVERFLOW);
    ASSERT_EQ(out_len, 0);
}

TEST(test_encode_overflow_zero_cap)
{
    uint8_t src[] = {1};
    uint8_t dst[1];
    size_t out_len = 99;
    ASSERT_EQ(rle_codec_encode(src, 1, dst, 0, &out_len), RLE_CODEC_ERR_OVERFLOW);
    ASSERT_EQ(out_len, 0);
}

/* ── encode: NULL params ────────────────────────────────────────── */

TEST(test_encode_null_src)
{
    uint8_t dst[4];
    size_t out_len;
    ASSERT_EQ(rle_codec_encode(NULL, 5, dst, sizeof(dst), &out_len), RLE_CODEC_ERR_PARAM);
}

TEST(test_encode_null_dst)
{
    uint8_t src[] = {1};
    size_t out_len;
    ASSERT_EQ(rle_codec_encode(src, 1, NULL, 4, &out_len), RLE_CODEC_ERR_PARAM);
}

TEST(test_encode_null_out_len)
{
    uint8_t src[] = {1};
    uint8_t dst[4];
    ASSERT_EQ(rle_codec_encode(src, 1, dst, sizeof(dst), NULL), RLE_CODEC_ERR_PARAM);
}

/* ── decode: basic ──────────────────────────────────────────────── */

TEST(test_decode_single_pair)
{
    uint8_t enc[] = {3, 0xAA};
    uint8_t dst[8];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_decode(enc, 2, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 3);
    ASSERT_EQ(dst[0], 0xAA);
    ASSERT_EQ(dst[1], 0xAA);
    ASSERT_EQ(dst[2], 0xAA);
}

TEST(test_decode_multiple_pairs)
{
    uint8_t enc[] = {2, 0x10, 3, 0x20, 1, 0x30};
    uint8_t dst[16];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_decode(enc, 6, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 6);
    ASSERT_EQ(dst[0], 0x10);
    ASSERT_EQ(dst[1], 0x10);
    ASSERT_EQ(dst[2], 0x20);
    ASSERT_EQ(dst[3], 0x20);
    ASSERT_EQ(dst[4], 0x20);
    ASSERT_EQ(dst[5], 0x30);
}

TEST(test_decode_empty)
{
    uint8_t dst[4];
    size_t out_len = 99;
    ASSERT_EQ(rle_codec_decode((const uint8_t *)"", 0, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 0);
}

/* ── decode: overflow ───────────────────────────────────────────── */

TEST(test_decode_overflow)
{
    uint8_t enc[] = {10, 0xAA};
    uint8_t dst[4]; /* need 10 but only 4 */
    size_t out_len = 99;
    ASSERT_EQ(rle_codec_decode(enc, 2, dst, 4, &out_len), RLE_CODEC_ERR_OVERFLOW);
    ASSERT_EQ(out_len, 0);
}

/* ── decode: odd length (invalid) ───────────────────────────────── */

TEST(test_decode_odd_length)
{
    uint8_t enc[] = {1, 2, 3};
    uint8_t dst[8];
    size_t out_len;
    ASSERT_EQ(rle_codec_decode(enc, 3, dst, sizeof(dst), &out_len), RLE_CODEC_ERR_PARAM);
}

/* ── decode: NULL params ────────────────────────────────────────── */

TEST(test_decode_null_src)
{
    uint8_t dst[4];
    size_t out_len;
    ASSERT_EQ(rle_codec_decode(NULL, 2, dst, sizeof(dst), &out_len), RLE_CODEC_ERR_PARAM);
}

TEST(test_decode_null_dst)
{
    uint8_t enc[] = {1, 0xAA};
    size_t out_len;
    ASSERT_EQ(rle_codec_decode(enc, 2, NULL, 4, &out_len), RLE_CODEC_ERR_PARAM);
}

TEST(test_decode_null_out_len)
{
    uint8_t enc[] = {1, 0xAA};
    uint8_t dst[4];
    ASSERT_EQ(rle_codec_decode(enc, 2, dst, sizeof(dst), NULL), RLE_CODEC_ERR_PARAM);
}

/* ── roundtrip ──────────────────────────────────────────────────── */

TEST(test_roundtrip_simple)
{
    uint8_t orig[] = {1, 1, 1, 2, 2, 3, 3, 3, 3};
    uint8_t enc[32], dec[32];
    size_t enc_len = 0, dec_len = 0;

    ASSERT_EQ(rle_codec_encode(orig, 9, enc, sizeof(enc), &enc_len), RLE_CODEC_OK);
    ASSERT_EQ(rle_codec_decode(enc, enc_len, dec, sizeof(dec), &dec_len), RLE_CODEC_OK);
    ASSERT_EQ(dec_len, 9);
    for (size_t i = 0; i < 9; i++) {
        ASSERT_EQ(dec[i], orig[i]);
    }
}

TEST(test_roundtrip_all_same)
{
    uint8_t orig[200];
    memset(orig, 0xFF, 200);
    uint8_t enc[8], dec[256];
    size_t enc_len = 0, dec_len = 0;

    ASSERT_EQ(rle_codec_encode(orig, 200, enc, sizeof(enc), &enc_len), RLE_CODEC_OK);
    ASSERT_EQ(rle_codec_decode(enc, enc_len, dec, sizeof(dec), &dec_len), RLE_CODEC_OK);
    ASSERT_EQ(dec_len, 200);
    for (size_t i = 0; i < 200; i++) {
        ASSERT_EQ(dec[i], 0xFF);
    }
}

TEST(test_roundtrip_all_different)
{
    uint8_t orig[8];
    for (uint8_t i = 0; i < 8; i++) { orig[i] = i; }
    uint8_t enc[32], dec[32];
    size_t enc_len = 0, dec_len = 0;

    ASSERT_EQ(rle_codec_encode(orig, 8, enc, sizeof(enc), &enc_len), RLE_CODEC_OK);
    ASSERT_EQ(enc_len, 16); /* worst case: 2x expansion */
    ASSERT_EQ(rle_codec_decode(enc, enc_len, dec, sizeof(dec), &dec_len), RLE_CODEC_OK);
    ASSERT_EQ(dec_len, 8);
    for (size_t i = 0; i < 8; i++) {
        ASSERT_EQ(dec[i], orig[i]);
    }
}

TEST(test_roundtrip_single_byte)
{
    uint8_t orig[] = {0x42};
    uint8_t enc[4], dec[4];
    size_t enc_len = 0, dec_len = 0;

    ASSERT_EQ(rle_codec_encode(orig, 1, enc, sizeof(enc), &enc_len), RLE_CODEC_OK);
    ASSERT_EQ(rle_codec_decode(enc, enc_len, dec, sizeof(dec), &dec_len), RLE_CODEC_OK);
    ASSERT_EQ(dec_len, 1);
    ASSERT_EQ(dec[0], 0x42);
}

TEST(test_roundtrip_long_run_split)
{
    /* 510 identical bytes → 2 pairs [255][val][255][val] → decode back */
    uint8_t orig[510];
    memset(orig, 0xAB, 510);
    uint8_t enc[8], dec[512];
    size_t enc_len = 0, dec_len = 0;

    ASSERT_EQ(rle_codec_encode(orig, 510, enc, sizeof(enc), &enc_len), RLE_CODEC_OK);
    ASSERT_EQ(enc_len, 4); /* [255,0xAB] [255,0xAB] */
    ASSERT_EQ(rle_codec_decode(enc, enc_len, dec, sizeof(dec), &dec_len), RLE_CODEC_OK);
    ASSERT_EQ(dec_len, 510);
    for (size_t i = 0; i < 510; i++) {
        ASSERT_EQ(dec[i], 0xAB);
    }
}

/* ── bound functions ────────────────────────────────────────────── */

TEST(test_encode_bound)
{
    ASSERT_EQ(rle_codec_encode_bound(0), 0);
    ASSERT_EQ(rle_codec_encode_bound(1), 2);
    ASSERT_EQ(rle_codec_encode_bound(100), 200);
}

TEST(test_decode_bound)
{
    ASSERT_EQ(rle_codec_decode_bound(0), 0);
    ASSERT_EQ(rle_codec_decode_bound(2), 255);
    ASSERT_EQ(rle_codec_decode_bound(4), 510);
    /* Odd → invalid → 0 */
    ASSERT_EQ(rle_codec_decode_bound(3), 0);
    ASSERT_EQ(rle_codec_decode_bound(1), 0);
}

/* ── decode: zero count pair (graceful) ─────────────────────────── */

TEST(test_decode_zero_count_graceful)
{
    /* [0, 0xAA] is not valid RLE but should not crash */
    uint8_t enc[] = {0, 0xAA, 2, 0xBB};
    uint8_t dst[8];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_decode(enc, 4, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    /* Zero-count pair skipped, only 2xBB emitted */
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ(dst[0], 0xBB);
    ASSERT_EQ(dst[1], 0xBB);
}

/* ── encode: exact fit ──────────────────────────────────────────── */

TEST(test_encode_exact_fit)
{
    uint8_t src[] = {5, 5, 5};
    uint8_t dst[2]; /* exactly 1 pair */
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 3, dst, 2, &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ(dst[0], 3);
    ASSERT_EQ(dst[1], 5);
}

/* ── decode: exact fit ──────────────────────────────────────────── */

TEST(test_decode_exact_fit)
{
    uint8_t enc[] = {3, 0x42};
    uint8_t dst[3]; /* exactly 3 bytes needed */
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_decode(enc, 2, dst, 3, &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 3);
    ASSERT_EQ(dst[0], 0x42);
    ASSERT_EQ(dst[2], 0x42);
}

/* ── error enum values ──────────────────────────────────────────── */

TEST(test_error_enum_values)
{
    ASSERT_EQ(RLE_CODEC_OK, 0);
    ASSERT_EQ(RLE_CODEC_ERR_PARAM, -1);
    ASSERT_EQ(RLE_CODEC_ERR_OVERFLOW, -2);
}

/* ── encode with byte value 0x00 ────────────────────────────────── */

TEST(test_encode_zeros)
{
    uint8_t src[] = {0, 0, 0, 0, 0};
    uint8_t dst[4];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 5, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    ASSERT_EQ(out_len, 2);
    ASSERT_EQ(dst[0], 5);
    ASSERT_EQ(dst[1], 0);
}

/* ── alternating bytes worst case ───────────────────────────────── */

TEST(test_encode_alternating)
{
    uint8_t src[] = {0xAA, 0xBB, 0xAA, 0xBB};
    uint8_t dst[16];
    size_t out_len = 0;
    ASSERT_EQ(rle_codec_encode(src, 4, dst, sizeof(dst), &out_len), RLE_CODEC_OK);
    /* No compression: 4 pairs */
    ASSERT_EQ(out_len, 8);
}

TEST_MAIN("rle_codec")
    RUN(test_encode_single_byte);
    RUN(test_encode_run_of_three);
    RUN(test_encode_mixed);
    RUN(test_encode_all_different);
    RUN(test_encode_empty);
    RUN(test_encode_run_split_255);
    RUN(test_encode_exactly_255);
    RUN(test_encode_256_bytes);
    RUN(test_encode_overflow_exact);
    RUN(test_encode_overflow_zero_cap);
    RUN(test_encode_null_src);
    RUN(test_encode_null_dst);
    RUN(test_encode_null_out_len);
    RUN(test_decode_single_pair);
    RUN(test_decode_multiple_pairs);
    RUN(test_decode_empty);
    RUN(test_decode_overflow);
    RUN(test_decode_odd_length);
    RUN(test_decode_null_src);
    RUN(test_decode_null_dst);
    RUN(test_decode_null_out_len);
    RUN(test_roundtrip_simple);
    RUN(test_roundtrip_all_same);
    RUN(test_roundtrip_all_different);
    RUN(test_roundtrip_single_byte);
    RUN(test_roundtrip_long_run_split);
    RUN(test_encode_bound);
    RUN(test_decode_bound);
    RUN(test_decode_zero_count_graceful);
    RUN(test_encode_exact_fit);
    RUN(test_decode_exact_fit);
    RUN(test_error_enum_values);
    RUN(test_encode_zeros);
    RUN(test_encode_alternating);
TEST_MAIN_END()
