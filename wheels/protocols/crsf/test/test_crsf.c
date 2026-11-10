// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "crsf.h"
#include <string.h>

/* Helper: build a valid RC channels frame */
static size_t build_rc_frame(uint8_t *buf, size_t cap, const crsf_channels_t *ch)
{
    crsf_frame_t f = { .addr = CRSF_ADDR_FC, .type = CRSF_TYPE_RC_CHANNELS };
    crsf_pack_channels(ch, f.payload, CRSF_CHANNEL_BYTES);
    f.payload_len = CRSF_CHANNEL_BYTES;
    size_t out_len = 0;
    crsf_encode_frame(&f, buf, cap, &out_len);
    return out_len;
}

/* ---------- CRC ---------- */

TEST(test_crc8_empty)
{
    ASSERT_EQ(crsf_crc8(NULL, 0), 0);
}

TEST(test_crc8_known)
{
    uint8_t data[] = {0x16};
    uint8_t crc = crsf_crc8(data, 1);
    ASSERT_TRUE(crc != 0);
}

/* ---------- frame decode ---------- */

TEST(test_decode_rc_frame)
{
    crsf_channels_t ch;
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++)
        ch.channels[i] = 992;
    uint8_t buf[64];
    size_t len = build_rc_frame(buf, sizeof(buf), &ch);
    crsf_frame_t f;
    ASSERT_EQ(crsf_decode_frame(buf, len, &f), CRSF_OK);
    ASSERT_EQ(f.addr, CRSF_ADDR_FC);
    ASSERT_EQ(f.type, CRSF_TYPE_RC_CHANNELS);
    ASSERT_EQ(f.payload_len, CRSF_CHANNEL_BYTES);
}

TEST(test_decode_null)
{
    crsf_frame_t f;
    ASSERT_EQ(crsf_decode_frame(NULL, 32, &f), CRSF_ERR_PARAM);
    uint8_t buf[32] = {0};
    ASSERT_EQ(crsf_decode_frame(buf, 32, NULL), CRSF_ERR_PARAM);
}

TEST(test_decode_short)
{
    /* A valid length byte promises 24 more bytes: waiting for them is right */
    uint8_t buf[4] = { CRSF_ADDR_FC, 24, CRSF_TYPE_RC_CHANNELS, 0 };
    crsf_frame_t f;
    ASSERT_EQ(crsf_decode_frame(buf, 0, &f), CRSF_ERR_SHORT);
    ASSERT_EQ(crsf_decode_frame(buf, 1, &f), CRSF_ERR_SHORT);
    ASSERT_EQ(crsf_decode_frame(buf, 2, &f), CRSF_ERR_SHORT);
    ASSERT_EQ(crsf_decode_frame(buf, 4, &f), CRSF_ERR_SHORT);
}

TEST(test_decode_bad_crc)
{
    crsf_channels_t ch = {{0}};
    uint8_t buf[64];
    size_t len = build_rc_frame(buf, sizeof(buf), &ch);
    buf[len - 1] ^= 0xFF;
    crsf_frame_t f;
    ASSERT_EQ(crsf_decode_frame(buf, len, &f), CRSF_ERR_CRC);
}

TEST(test_decode_truncated)
{
    crsf_channels_t ch = {{0}};
    uint8_t buf[64];
    size_t len = build_rc_frame(buf, sizeof(buf), &ch);
    crsf_frame_t f;
    ASSERT_EQ(crsf_decode_frame(buf, len - 5, &f), CRSF_ERR_SHORT);
}

/* ---------- frame encode ---------- */

TEST(test_encode_ok)
{
    crsf_frame_t f = { .addr = CRSF_ADDR_FC, .type = 0x14, .payload_len = 2 };
    f.payload[0] = 0xAA;
    f.payload[1] = 0xBB;
    uint8_t buf[64];
    size_t out = 0;
    ASSERT_EQ(crsf_encode_frame(&f, buf, sizeof(buf), &out), CRSF_OK);
    ASSERT_EQ(out, 6u);
    ASSERT_EQ(buf[0], CRSF_ADDR_FC);
    ASSERT_EQ(buf[1], 4);
    ASSERT_EQ(buf[2], 0x14);
}

TEST(test_encode_null)
{
    uint8_t buf[64];
    size_t out = 0;
    crsf_frame_t f = {0};
    ASSERT_EQ(crsf_encode_frame(NULL, buf, 64, &out), CRSF_ERR_PARAM);
    ASSERT_EQ(crsf_encode_frame(&f, NULL, 64, &out), CRSF_ERR_PARAM);
    ASSERT_EQ(crsf_encode_frame(&f, buf, 64, NULL), CRSF_ERR_PARAM);
}

TEST(test_encode_overflow)
{
    crsf_frame_t f = { .payload_len = 50 };
    uint8_t buf[8];
    size_t out = 0;
    ASSERT_EQ(crsf_encode_frame(&f, buf, sizeof(buf), &out), CRSF_ERR_OVERFLOW);
}

/* ---------- channel pack/unpack ---------- */

TEST(test_channel_roundtrip)
{
    crsf_channels_t src;
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++)
        src.channels[i] = (uint16_t)(172 + i * 100);
    uint8_t packed[CRSF_CHANNEL_BYTES];
    ASSERT_EQ(crsf_pack_channels(&src, packed, sizeof(packed)), CRSF_OK);
    crsf_channels_t dst;
    ASSERT_EQ(crsf_unpack_channels(packed, sizeof(packed), &dst), CRSF_OK);
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++)
        ASSERT_EQ(dst.channels[i], src.channels[i]);
}

TEST(test_channel_center)
{
    crsf_channels_t src;
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++)
        src.channels[i] = 992;
    uint8_t packed[CRSF_CHANNEL_BYTES];
    crsf_pack_channels(&src, packed, sizeof(packed));
    crsf_channels_t dst;
    crsf_unpack_channels(packed, sizeof(packed), &dst);
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++)
        ASSERT_EQ(dst.channels[i], 992);
}

TEST(test_channel_extremes)
{
    crsf_channels_t src;
    src.channels[0] = 172;
    src.channels[1] = 1811;
    for (int i = 2; i < CRSF_NUM_CHANNELS; i++)
        src.channels[i] = 0;
    uint8_t packed[CRSF_CHANNEL_BYTES];
    crsf_pack_channels(&src, packed, sizeof(packed));
    crsf_channels_t dst;
    crsf_unpack_channels(packed, sizeof(packed), &dst);
    ASSERT_EQ(dst.channels[0], 172);
    ASSERT_EQ(dst.channels[1], 1811);
}

TEST(test_unpack_null)
{
    crsf_channels_t c;
    ASSERT_EQ(crsf_unpack_channels(NULL, 22, &c), CRSF_ERR_PARAM);
    uint8_t p[22] = {0};
    ASSERT_EQ(crsf_unpack_channels(p, 22, NULL), CRSF_ERR_PARAM);
}

TEST(test_unpack_short)
{
    uint8_t p[10] = {0};
    crsf_channels_t c;
    ASSERT_EQ(crsf_unpack_channels(p, 10, &c), CRSF_ERR_SHORT);
}

TEST(test_pack_null)
{
    uint8_t p[22];
    crsf_channels_t c = {{0}};
    ASSERT_EQ(crsf_pack_channels(NULL, p, 22), CRSF_ERR_PARAM);
    ASSERT_EQ(crsf_pack_channels(&c, NULL, 22), CRSF_ERR_PARAM);
}

TEST(test_pack_overflow)
{
    crsf_channels_t c = {{0}};
    uint8_t p[10];
    ASSERT_EQ(crsf_pack_channels(&c, p, 10), CRSF_ERR_OVERFLOW);
}

/* ---------- full frame roundtrip ---------- */

TEST(test_full_roundtrip)
{
    crsf_channels_t src;
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++)
        src.channels[i] = (uint16_t)(200 + i * 99);
    uint8_t buf[64];
    size_t len = build_rc_frame(buf, sizeof(buf), &src);
    ASSERT_TRUE(len > 0);

    crsf_frame_t f;
    ASSERT_EQ(crsf_decode_frame(buf, len, &f), CRSF_OK);
    crsf_channels_t dst;
    ASSERT_EQ(crsf_unpack_channels(f.payload, f.payload_len, &dst), CRSF_OK);
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++)
        ASSERT_EQ(dst.channels[i], src.channels[i]);
}

/* ---------- error codes ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ((int)CRSF_OK, 0);
    ASSERT_EQ((int)CRSF_ERR_PARAM, -1);
    ASSERT_EQ((int)CRSF_ERR_SHORT, -2);
    ASSERT_EQ((int)CRSF_ERR_CRC, -3);
    ASSERT_EQ((int)CRSF_ERR_OVERFLOW, -4);
    ASSERT_EQ((int)CRSF_ERR_FORMAT, -5);
}

/* ---------- fuzz ---------- */

TEST(test_fuzz)
{
    crsf_frame_t f;
    unsigned seed = 42;
    for (int i = 0; i < 1000; i++) {
        uint8_t buf[128];
        int len = (int)(seed % 65);
        for (int j = 0; j < len; j++) {
            seed = seed * 1103515245u + 12345u;
            buf[j] = (uint8_t)(seed >> 16);
        }
        seed = seed * 1103515245u + 12345u;
        crsf_decode_frame(buf, (size_t)len, &f);
    }
}

TEST(test_encode_rejects_payload_len_past_array)
{
    crsf_frame_t frame;
    uint8_t buf[256];
    size_t out_len = 0;

    memset(&frame, 0, sizeof(frame));
    frame.payload_len = CRSF_MAX_FRAME_SIZE + 1;

    ASSERT_EQ(crsf_encode_frame(&frame, buf, sizeof(buf), &out_len), CRSF_ERR_OVERFLOW);
}

/* ---------- frame limits ---------- */

/* tbs-crsf-spec crsf.md: "Each CRSF frame is not longer than 64 bytes (including
   the Sync and CRC bytes)", and of the length byte: "Valid range is between 2
   and 62. If this uint8 value is out of valid range the frame must be
   discarded." Sync, length, type and CRC leave 60 bytes of payload. */

/* A frame with this length byte and a good CRC. Returns its size. */
static size_t build_with_length_byte(uint8_t *buf, uint8_t length_byte)
{
    buf[0] = CRSF_ADDR_FC;
    buf[1] = length_byte;
    buf[2] = 0x14;
    for (size_t i = 3; i + 1U < (size_t)length_byte + 2U; i++) {
        buf[i] = (uint8_t)i;
    }
    buf[length_byte + 1U] = crsf_crc8(&buf[2], (size_t)length_byte - 1U);
    return (size_t)length_byte + 2U;
}

TEST(test_encode_stops_at_a_64_byte_frame)
{
    crsf_frame_t f;
    uint8_t buf[256];
    size_t out = 0;
    memset(&f, 0, sizeof(f));
    f.addr = CRSF_ADDR_FC;
    f.type = 0x14;

    f.payload_len = 60;
    ASSERT_EQ(crsf_encode_frame(&f, buf, sizeof(buf), &out), CRSF_OK);
    ASSERT_EQ(out, 64U);
    ASSERT_EQ(buf[1], 62);

    f.payload_len = 61;
    ASSERT_EQ(crsf_encode_frame(&f, buf, sizeof(buf), &out), CRSF_ERR_OVERFLOW);
    f.payload_len = 64;
    ASSERT_EQ(crsf_encode_frame(&f, buf, sizeof(buf), &out), CRSF_ERR_OVERFLOW);
}

TEST(test_decode_takes_both_ends_of_the_valid_range)
{
    uint8_t buf[300];
    crsf_frame_t f;

    size_t total = build_with_length_byte(buf, 62);
    ASSERT_EQ(total, 64U);
    ASSERT_EQ(crsf_decode_frame(buf, total, &f), CRSF_OK);
    ASSERT_EQ(f.payload_len, 60);
    ASSERT_EQ(f.payload[59], 62);

    total = build_with_length_byte(buf, 2);
    ASSERT_EQ(total, 4U);
    ASSERT_EQ(crsf_decode_frame(buf, total, &f), CRSF_OK);
    ASSERT_EQ(f.payload_len, 0);
}

TEST(test_decode_discards_a_length_byte_past_62)
{
    static const uint8_t lengths[] = { 63, 64, 65, 66, 67, 200, 255 };
    uint8_t buf[300];
    crsf_frame_t f;

    for (size_t i = 0; i < sizeof(lengths); i++) {
        size_t total = build_with_length_byte(buf, lengths[i]);
        ASSERT_EQ(crsf_decode_frame(buf, total, &f), CRSF_ERR_FORMAT);
        /* The byte alone settles it: no more data can make this frame good */
        ASSERT_EQ(crsf_decode_frame(buf, 2, &f), CRSF_ERR_FORMAT);
    }
}

TEST(test_decode_discards_a_length_byte_under_2)
{
    uint8_t zero[8] = { CRSF_ADDR_FC, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t one[8]  = { CRSF_ADDR_FC, 1, 0, 0, 0, 0, 0, 0 };
    crsf_frame_t f;

    ASSERT_EQ(crsf_decode_frame(zero, sizeof(zero), &f), CRSF_ERR_FORMAT);
    ASSERT_EQ(crsf_decode_frame(one, sizeof(one), &f), CRSF_ERR_FORMAT);
    ASSERT_EQ(crsf_decode_frame(zero, 2, &f), CRSF_ERR_FORMAT);
    ASSERT_EQ(crsf_decode_frame(one, 2, &f), CRSF_ERR_FORMAT);
}

TEST_MAIN("crsf")
    RUN(test_crc8_empty);
    RUN(test_crc8_known);
    RUN(test_decode_rc_frame);
    RUN(test_decode_null);
    RUN(test_decode_short);
    RUN(test_decode_bad_crc);
    RUN(test_decode_truncated);
    RUN(test_encode_ok);
    RUN(test_encode_null);
    RUN(test_encode_overflow);
    RUN(test_channel_roundtrip);
    RUN(test_channel_center);
    RUN(test_channel_extremes);
    RUN(test_unpack_null);
    RUN(test_unpack_short);
    RUN(test_pack_null);
    RUN(test_pack_overflow);
    RUN(test_full_roundtrip);
    RUN(test_error_codes);
    RUN(test_fuzz);
    RUN(test_encode_rejects_payload_len_past_array);
    RUN(test_encode_stops_at_a_64_byte_frame);
    RUN(test_decode_takes_both_ends_of_the_valid_range);
    RUN(test_decode_discards_a_length_byte_past_62);
    RUN(test_decode_discards_a_length_byte_under_2);
TEST_MAIN_END()
